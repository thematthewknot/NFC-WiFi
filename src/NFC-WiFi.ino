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

bool startScanning = false;
bool noClientConnected= true;
int numTags; //default number of tags 
int timezone;





void spiffsWrite(String, String);
String host;
String IFTTTKey;

String storedTags[MAXNUMTAGS] = {};
String storedEvents[MAXNUMTAGS] = {};
String spiffsRead(String);
void LED_Off();
void LED_Blue();
void LED_Red();
void LED_Green();
void nfcread();
void UIDrecord(int);
void ToggleEvent(String);
void StoreTagsBeforeStart();
void send302(String);
void fetchURL(BearSSL::WiFiClientSecure, const char , const uint16_t, const char );

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
  <a href="/">Tags/Events</a>
  <a href="/setup">Setup</a>
  <a href="/debug">Debug</a>
</div>
)";

void setup()
{ 
  Serial.begin(115200);
  SPIFFS.begin();
  


  if ( ! SPIFFS.exists("/uid1str") ) {
    spiffsWrite("/uid1str", "Not Set");
  }
  if ( ! SPIFFS.exists("/event1str") ) {
    spiffsWrite("/event1str", "Not Set");
  }
  if ( ! SPIFFS.exists("/numTags") ) {
    spiffsWrite("/numTags", String(1));
  }
  numTags = spiffsRead("/numTags").toInt();

  if( ! SPIFFS.exists("/timezone")){
    spiffsWrite("/timezone", String(-4));
  }
  timezone = spiffsRead("/timezone").toInt();
  if( ! SPIFFS.exists("/IFTTThost")){
    spiffsWrite("/IFTTThost", "maker.ifttt.com");
  }
  host = spiffsRead("/IFTTThost");
  if( ! SPIFFS.exists("/IFTTTKEY")){
    spiffsWrite("/IFTTTKEY", "Not Set");
  }
  IFTTTKey = spiffsRead("/IFTTTKEY");

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
      <p>For first use click on Setup and go set your IFTTT webhook key.</p>
      <p>After you've set up the tags and events you wish to use hit run to start using the NFC-WiFi Board</p>
      <p>The board will automatically start running when powered up after 2min of no connection to the webpage</p>
      <h4>IFTTT Key</h4>
      <form method="POST" id="setup-device" action="/setup/IFTTTKEY">
        <input name="iftttkey" placeholder="IFTTTKey" value=")" + spiffsRead("/IFTTTKEY") + R"(">
        <button type="submit">Save</button>
      </form>
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
     <p>Click the register button and scan a tag once the scanner light goes blue and wait for the page to reload.</p>
     <p>Once the page reloads(be patient) enter the IFTTT event you want the tag to toggle and hit save.</p>
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
      <h4>Event )";
      content +=String(i);
       content += R"(</h4>
      <form method='POST' id='Eventrec' action='/Eventrec'>
        <input name=")";
                content += String(i);
         content += R"(" placeholder="Not Set" value=")";
        content +=spiffsRead("/event"+String(i));
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

server.on("/Eventrec", HTTP_POST, [&]() {
  String tempURLIndex = server.argName(0);

String EventStr =  server.arg(tempURLIndex);
 
  
  int EventIndex = tempURLIndex.toInt();
  Serial.println("event num:"+String(EventIndex));
  Serial.println("event is:"+EventStr);
  spiffsWrite("/event" + String(EventIndex), EventStr);
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
server.on("/setup/IFTTTKEY", HTTP_POST, [&]() {
  Serial.print("[httpd] timezone settings post. ");
  spiffsWrite("/IFTTTKEY", server.arg("iftttkey"));
  IFTTTKey = spiffsRead("/IFTTTKEY");
  send302("/");
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
WiFi.hostname("NFC-WiFi");


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
      storedEvents[i]=spiffsRead("/event"+String(i+1));
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
           ToggleEvent(i);
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

void fetchURL(BearSSL::WiFiClientSecure *client, const char *host, const uint16_t port, const char *path) {
  if (!path) {
    path = "/";
  }

  ESP.resetFreeContStack();
  uint32_t freeStackStart = ESP.getFreeContStack();
  Serial.printf("Trying: %s:443...", host);
  client->connect(host, port);
  if (!client->connected()) {
    Serial.printf("*** Can't connect. ***\n-------\n");
    return;
  }
  Serial.printf("Connected!\n-------\n");
  client->write("GET ");
  client->write(path);
  client->write(" HTTP/1.0\r\nHost: ");
  client->write(host);
  client->write("\r\nUser-Agent: ESP8266\r\n");
  client->write("\r\n");
  uint32_t to = millis() + 5000;
  if (client->connected()) {
    do {
      char tmp[32];
      memset(tmp, 0, 32);
      int rlen = client->read((uint8_t*)tmp, sizeof(tmp) - 1);
      yield();
      if (rlen < 0) {
        break;
      }
      // Only print out first line up to \r, then abort connection
      char *nl = strchr(tmp, '\r');
      if (nl) {
        *nl = 0;
        Serial.print(tmp);
        break;
      }
      Serial.print(tmp);
    } while (millis() < to);
  }
  client->stop();
  uint32_t freeStackEnd = ESP.getFreeContStack();
  Serial.printf("\nCONT stack used: %d\n-------\n\n", freeStackStart - freeStackEnd);
}



void ToggleEvent(int Event_index)
{
   String event = storedEvents[Event_index];
   

   Serial.println("Attempting to call:"+event);

      WiFi.mode(WIFI_STA);
 //WiFi.begin(ssid, pass);
   String url = "/trigger/"+event+"/with/key/"+IFTTTKey;
  

  BearSSL::WiFiClientSecure client;
  client.setInsecure();
  fetchURL(&client, host.c_str(), 443, url.c_str());
   
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