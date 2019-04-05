// NFC-WiFi board simple program 2018 Matt Varian
// https://github.com/thematthewknot/NFC-WiFi
// released under the GPLv3 license.
#include <FS.h>
#include <ESP8266WiFi.h> 
#include <ESP8266HTTPClient.h>
#include <WiFiManager.h>
#include <APA102.h>
#include <Adafruit_PN532.h>
#include <time.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>


class SPIFFSCertStoreFile : public BearSSL::CertStoreFile {
  public:
    SPIFFSCertStoreFile(const char *name) {
      _name = name;
    };
    virtual ~SPIFFSCertStoreFile() override {};
    // The main API
    virtual bool open(bool write = false) override {
      _file = SPIFFS.open(_name, write ? "w" : "r");
      return _file;
    }
    virtual bool seek(size_t absolute_pos) override {
      return _file.seek(absolute_pos, SeekSet);
    }
    virtual ssize_t read(void *dest, size_t bytes) override {
      return _file.readBytes((char*)dest, bytes);
    }
    virtual ssize_t write(void *dest, size_t bytes) override {
      return _file.write((uint8_t*)dest, bytes);
    }
    virtual void close() override {
      _file.close();
    }
  private:
    File _file;
    const char *_name;
};
SPIFFSCertStoreFile certs_idx("/certs.idx");
SPIFFSCertStoreFile certs_ar("/certs.ar");




ESP8266WebServer server(80);

const uint8_t clockPin = 4;
const uint8_t dataPin = 5;

#define PN532_SCK  (16)
#define PN532_MOSI (12)
#define PN532_SS   (13)
#define PN532_MISO (14)
#define VERSION      1
#define MAXNUMTAGS  10 //Max number of tags


uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };

Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);
APA102<dataPin, clockPin> ledStrip;

//const uint16_t ledCount = 1;
//const uint8_t brightness = 1;

File fsUploadFile;              // a File object to temporarily store the received file
bool startScanning = false;
bool noClientConnected= true;
int numTags; //default number of tags 
int timezone;
const char * header = R"(<!DOCTYPE html>
<html>
<head>
<title>NFC-WiFi</title>
<link rel="stylesheet" href="/style.css">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no" />
</head>
<body>
<div id="top">
  <span id="title">NFC-WiFi</span>
  <a href="/">Tags</a>
  <a href="/setup">Setup</a>
  <a href="/debug">Debug</a>
  <a href="/update">Update</a>
</div>
)";




void spiffsWrite(String, String);
String storedTags[MAXNUMTAGS] = {};
String storedURLs[MAXNUMTAGS] = {};
String spiffsRead(String);
int CheckNumCerts();
void LED_Off();
void LED_Blue();
void LED_Red();
void LED_Green();
void nfcread();
void UIDrecord(int);
void UseURL(String);
void StoreTagsBeforeStart();
void send302(String);

void setup()
{ 
  Serial.begin(115200);
  SPIFFS.begin();
  


  if ( ! SPIFFS.exists("/uid1str") ) {
    spiffsWrite("/uid1str", "Not Set");
  }
  if ( ! SPIFFS.exists("/url1str") ) {
    spiffsWrite("/url1str", "Not Set");
  }
  if ( ! SPIFFS.exists("/numTags") ) {
    spiffsWrite("/numTags", String(1));
  }
  numTags = spiffsRead("/numTags").toInt();

  if( ! SPIFFS.exists("/timezone")){
    spiffsWrite("/timezone", String(-4));
  }
  timezone = spiffsRead("/timezone").toInt();
 

  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.print("Didn't find PN53x board");
    while (1); 
  }

  Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX);
  Serial.print("Firmware ver. "); Serial.print((versiondata>>16) & 0xFF, DEC);
  Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);

  nfc.SAMConfig();
  LED_Off();

server.on("/update", HTTP_GET, [&](){
    String content = header;
    content += ("<h1>Update</h1><p>Number of certs read from the cert.ar file currently loaded = ");

      content += String(CheckNumCerts());


    content += R"(
      </p><h2>Update cert from a file</h2>
      <p>Run this python script, and upload the cert.ar file if need be(or you can upload your own if you want)</p>
      <p>https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/examples/BearSSL_CertStore/certs-from-mozilla.py</p>
      <p>This shouldn't be required, but you never know. If using your own use cert.ar filename</p>
      <form method='POST' action='/update' enctype='multipart/form-data'>
        <input type='file' name='update'>
        <input type='submit' value='Do it'>
      </form>
    )";

server.send(200, "text/html", content);
});
// handler for the /update form POST (once file upload finishes)
server.on("/update", HTTP_POST, [&]() {
  server.sendHeader("Connection", "close");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
}, [&]() {
  // handler for the file upload, get's the sketch bytes, and writes
  // them through the Update object
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) filename = "/" + filename;
    Serial.print("handleFileUpload Name: "); Serial.println(filename);
    fsUploadFile = SPIFFS.open(filename, "w");            // Open the file for writing in SPIFFS (create if it doesn't exist)
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {                                   // If the file was successfully created
      fsUploadFile.close();                               // Close the file again
      Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
      server.sendHeader("Location", "/success.html");     // Redirect the client to the success page
      server.send(303);
    } else {
      server.send(500, "text/plain", "500: couldn't create file");
    }
  }
  delay(0);
});
server.on("/style.css", [&]() {
  server.send(200, "text/css", R"(
      html {
        font-family:sans-serif;
        background-color:black;
        color: #e0e0e0;
      }
      div {
        background-color: #202020;
      }
      h1,h2,h3,h4,h5 {
        color: #e02020;
      }
      a {
        color: #f05050;
        margin-left:12px;
      }
      form *, button {
        display:block;
        border: 1px solid #000;
        font-size: 14px;
        color: #fff;
        background: #444;
        padding: 5px;
        margin-bottom:12px;
      }
      #color-buttons {
        display:table;
        max-width:144px;
      }
      .color-button {
        width:36px;
        height:36px;
        display:inline;
        margin:0px !important;
      }
    )");
});
server.on("/", [&]() {

  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  noClientConnected = false;
  String content = header;


  content += R"(
      </select>
      <!-- <button type="submit">Set</button> -->
      <p>After you've set up the tags you wish to use hit run to start using the NFC-WiFi Board</p>
      <form method="POST" id="Run" action="/RunScan">
        <button type="submit">RUN</button>
      </form>    
      <p>Set The number of tags you want to use, then select set tag number</p>
      <form method="POST" id="SetTagNum" action="/setNumTags">
        <select name="numberOfTags">
          <option value="1">1</option>
          <option value="2">2</option>
          <option value="3">3</option>
          <option value="4">4</option>
          <option value="5">5</option>
          <option value="6">6</option>
          <option value="7">7</option>
          <option value="8">8</option>
          <option value="9">9</option>
          <option value="10">10</option>
        </select>
        <button type="submit">Set Tag Number</button>

      </form>
     <p>Below is to setup your tags, click the register button and scan a tag</p>
     <p>once the page reloads(be patient) paste the URL you want to toggle and hit save for each tag </p>

  )";
  for(int i=1;i<numTags+1;i++)
  {
    content += R"(
     <h4>Tag )";
        content += String(i);  
        content += R"(</h4>
      <form method="POST" id="UIDrec" action="/UIDrec">
        <input name=")";
        content += String(i);
        content += R"(" placeholder="Not Set" value=")";
        content += spiffsRead("/uid"+String(i)+"str");
        content += R"("">
        <button type="submit">Register</button>
      </form>       )"; 
        
     content += R"(  
      <h4>URL )";
      content +=String(i);
       content += R"(</h4>
      <form method='POST' id='URLrec' action='/URLrec'>
        <input name=")";
                content += String(i);
         content += R"(" placeholder="Not Set" value=")";
        content +=spiffsRead("/url"+String(i)+"str");
         content += R"("">
        <button type='submit'>Save</button>
      </form>        
    )";
  
  }
  
  server.send(200, "text/html", content);
});
server.on("/RunScan", HTTP_POST, [&]() {
   

  StoreTagsBeforeStart();

    Serial.println("endering run state");
});
server.on("/setNumTags", HTTP_POST, [&]() {

    String NumTagsStr = server.arg(0);
    numTags =NumTagsStr.toInt();
  spiffsWrite("/numTags",NumTagsStr);    
  send302("/");

    
});
server.on("/UIDrec", HTTP_POST, [&]() {
  String tempUIDIndex = server.argName(0);
  int uidindex = tempUIDIndex.toInt();
  UIDrecord(uidindex);
  server.sendHeader("Location", "/", true);
  server.send ( 302, "text/plain", "");
  //send302("/");
});

server.on("/URLrec", HTTP_POST, [&]() {
  String tempURLIndex = server.argName(0);

String urlStr =  server.arg(tempURLIndex);
 
  
  int urlIndex = tempURLIndex.toInt();
  spiffsWrite("/url" + String(urlIndex) + "str", urlStr);
  send302("/");
});


server.on("/setup", [&]() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.sendHeader("Content-Length", "-1");
  server.send(200, "text/html", header);

  server.sendContent("\
      <h1>Setup</h1>\
      <h4>Nearby networks</h4>\
      <table>\
      <tr><th>Name</th><th>Security</th><th>Signal</th></tr>\
    ");
  Serial.println("[httpd] scan start");
  int n = WiFi.scanNetworks();
  Serial.println("[httpd] scan done");
  for (int i = 0; i < n; i++) {
    server.sendContent(String() + "\r\n<tr onclick=\"document.getElementById('ssidinput').value=this.firstChild.firstChild.innerHTML; setTimeout(function(){ document.getElementById('pskinput').focus(); }, 100);\"><td><a href=\"#setup-wifi\">" + WiFi.SSID(i) + "</a></td><td>" + String((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? "Open" : "Encrypted") + "</td><td>" + WiFi.RSSI(i) + "dBm</td></tr>");
  }
  server.sendContent(String() + "\
      </table>\
      <h4>Connect to a network</h4>\
      <form method='POST' id='setup-wifi' action='/setup/wifi'>\
        <input type='text' id='ssidinput' placeholder='network' value='" + String(WiFi.SSID()) + "' name='n'>\
        <input type='password' id='pskinput' placeholder='password' value='" + String(WiFi.psk()) + "' name='p'>\
        <button type='submit'>Save and Connect</button>\
      </form>\
    ");

  server.sendContent(String() + R"(
      <h4>Device Name</h4>
      <form method="POST" id="setup-device" action="/setup/device">
        <input name="name" placeholder="Device name" value=")" + spiffsRead("/name") + R"(">
        <button type="submit">Save</button>
      </form>
    )");

  server.sendContent(String() + R"(
      <h4>TimeZone</h4>
      <form method="POST" id="setup-device" action="/setup/timezone">
        <input name="tzname" placeholder="timezone" value=")" + spiffsRead("/timezone") + R"(">
        <button type="submit">Save</button>
      </form>
    )");
  server.client().stop();
});
server.on("/setup/timezone", HTTP_POST, [&]() {
  Serial.print("[httpd] timezone settings post. ");
  spiffsWrite("/timezone", server.arg("tzname"));
  timezone = spiffsRead("/timezone").toInt();
  send302("/setup?saved");
  Serial.println("done.");
});
server.on("/setup/device", HTTP_POST, [&]() {
  Serial.print("[httpd] Device settings post. ");
  spiffsWrite("/name", server.arg("name"));

  send302("/setup?saved");
  Serial.println("done.");
});
server.on("/debug", [&]() {
  String content = header;
  content += ("<h1>About</h1><ul>");

  unsigned long uptime = millis();
  content += (String("<li>Version ") + VERSION + "</li>");
  content += (String("<li>Booted about ") + (uptime / 60000) + " minutes ago (" + ESP.getResetReason() + ")</li>");
  content += ("</ul>");


  server.send(200, "text/html", content);
});

server.on("/version.json", [&]() {
  server.send(200, "text/html", String("1"));
  server.client().stop();
});
//  server.onNotFound ( handleNotFound );



WiFiManager wifiManager;
wifiManager.autoConnect("NFC_WiFi");
//setClock();
 
server.begin(); // Web server start
Serial.println("End Of Setup Loop");


}




void loop() {

  if(noClientConnected) //timeout of 2min before starting to run if no one connects to website.
   { 
    if( millis()/120000) 
    {
      StoreTagsBeforeStart();   
    }
   }  


  if(startScanning){
    server.stop();
    nfcread();
  }
  else{
    server.handleClient();
    }
}

void StoreTagsBeforeStart(){ //read all thags and urls before starting 
  noClientConnected =false;
  startScanning = true;
  for(int i=0;i<numTags;i++)
  {
      storedTags[i]=spiffsRead("/uid"+String(i+1)+"str");
      storedURLs[i]=spiffsRead("/url"+String(i+1)+"str");
  }
}

void nfcread(){
  server.stop();
  WiFi.mode(WIFI_STA);
  while(true){
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;              // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
  
  
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
  
  if (success) {
    //Serial.println("Found an ISO14443A card");
    //Serial.print("  UID Length: ");    Serial.print(uidLength, DEC);   Serial.println(" bytes");
    //Serial.print("  UID Value: ");   
    //nfc.PrintHex(uid, uidLength);   
    //Serial.println("");
    String readuid = "";
    for(int i=0;i<uidLength;i++)
    {
      readuid = readuid + uid[i];
    }
    Serial.println(readuid);
   
    bool isMatch= false;
    for(int i=0;i<numTags;i++)
    {
        //Serial.println("DEBUG list:"+storedTags[i]);
       
        if(readuid==storedTags[i])
        {
            LED_Green();
           UseURL(i);
          delay(5000);
           LED_Off();
           isMatch = true;
          break;
        }
               
    }
      if(isMatch == false)
      {
        LED_Red();
        delay(3000);
        LED_Off();
      }

 
      
  }
  }
}

void UIDrecord(int index_num) {
  LED_Blue();
  uint8_t success;
  uint8_t uidLength;
  String tempstr = "";
  bool waitforread = true;
  while (waitforread) {
    success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

    if (success) {
      waitforread = false;
      Serial.println("Found an ISO14443A card");
      Serial.print("  UID Length: "); Serial.print(uidLength, DEC); Serial.println(" bytes");
      Serial.print("  UID Value: ");
      nfc.PrintHex(uid, uidLength);
      Serial.println(" saving to UID slot: " + index_num);
      Serial.println("*************");
      for (int i = 0; i < uidLength; i++)
        tempstr = tempstr + uid[i];
      Serial.print("using index:");
      Serial.println(index_num);
      spiffsWrite("/uid" + String(index_num) + "str", tempstr);

    }
  }
  LED_Off();
  delay(10);
}


void UseURL(int url_index)
{
   String url = storedURLs[url_index];
   String url_first4= url.substring(0,5);
   url_first4.toLowerCase();
   Serial.println("Attempting to call:"+url);
   Serial.println("checking http vs https got:"+url_first4);
   if(url_first4 == "https"){
   //WiFiManager wifiManager;
  //if (wifiManager.autoConnect()) 
  if(WiFi.status() == WL_CONNECTED){
    Serial.printf("before clock heap size: %u\n", ESP.getFreeHeap());
    setClock();
    Serial.printf("after clock heap size: %u\n", ESP.getFreeHeap());
    //SPIFFS.begin();
    HTTPClient http;
    BearSSL::WiFiClientSecure *client = new BearSSL::WiFiClientSecure();
    BearSSL::CertStore certStore;
    int numCerts = certStore.initCertStore(&certs_idx, &certs_ar);
    client->setCertStore(&certStore);
    Serial.println(numCerts);
    Serial.printf("after cert heap size: %u\n", ESP.getFreeHeap());

    for(int i=0;i<10;i++){
      http.begin(dynamic_cast<WiFiClient&>(*client), url);
        Serial.printf("heap size before get: %u\n", ESP.getFreeHeap());

      long httpCode = http.GET();
      
      Serial.printf("heap size after get: %u\n", ESP.getFreeHeap());
      delay(1000);
      Serial.println(httpCode);
      Serial.print("result");
      Serial.println(http.getString());
      if(httpCode>0)
        break;
      delay(100);
    }
 } else {
    Serial.println("Failed to connect to Wifi.");
  }
  Serial.println("done");
}
else
{
  Serial.println("trying regular http post of:" + url);
   WiFiClient client;

    HTTPClient http;

    Serial.print("[HTTP] begin...\n");
    if (http.begin(client, url)) {  // HTTP


      Serial.print("[HTTP] GET...\n");
      // start connection and send HTTP header
      int httpCode = http.GET();

      // httpCode will be negative on error
      if (httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);
        // file found at server
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = http.getString();
          Serial.println(payload);
        }
      } else {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }

      http.end();
    } else {
      Serial.printf("[HTTP} Unable to connect\n");
    }

}
}

int CheckNumCerts(){
    BearSSL::WiFiClientSecure *client = new BearSSL::WiFiClientSecure();
    BearSSL::CertStore certStore;
    int numCerts = certStore.initCertStore(&certs_idx, &certs_ar);
    return numCerts;

}

void spiffsWrite(String path, String contents) {
  Serial.println("SPIFFS Path:" + path);
  Serial.println("SPIFFS contents:" + contents);
  File f = SPIFFS.open(path, "w");
  f.print(contents);
  f.close();
}
String spiffsRead(String path) {
  File f = SPIFFS.open(path, "r");
  String x = f.readStringUntil('\n');
  f.close();
  return x;
}

void LED_Blue()
{
  ledStrip.startFrame();
  ledStrip.sendColor(0, 0, 255, 1);
  ledStrip.endFrame(1);
}
void LED_Green()
{
  ledStrip.startFrame();
  ledStrip.sendColor(0, 255, 0, 1);
  ledStrip.endFrame(1);
}
void LED_Red()
{
  ledStrip.startFrame();
  ledStrip.sendColor(255, 0, 0, 1);
  ledStrip.endFrame(1);
}
void LED_Off()
{
  ledStrip.startFrame();
  ledStrip.sendColor(0, 0, 0, 1);
  ledStrip.endFrame(1);
}
void send302(String dest) {
  server.sendHeader("Location", dest, true);
  server.send ( 302, "text/plain", "");
  server.client().stop();
}
void setClock() {
  configTime(timezone * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
}
