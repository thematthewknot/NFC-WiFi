// NFC-WiFi board simple program 2018 Matt Varian
// https://github.com/thematthewknot/NFC-WiFi
// released under the GPLv3 license.
#include <FS.h>
#include <WiFiManager.h>
#include <APA102.h>
#include <Adafruit_PN532.h>
#include <Wire.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <CertStoreBearSSL.h>
#include <time.h>


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



uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 }; 
String uid1str = "Not Set";
String url1str = "Not Set";
String IFTTTKey = "Not Set";
String IFTTTEvent1 = "Not Set";
//String host = "maker.ifttt.com";
//int httpsPort = 443;
//char fingerprint[] = "AA:75:CB:41:2E:D5:F9:97:FF:5D:A0:8B:7D:AC:12:21:08:4B:00:8C";

Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);
APA102<dataPin, clockPin> ledStrip;

const uint16_t ledCount = 1;
const uint8_t brightness = 1;

String notice;

  
const char MAIN_page1[]PROGMEM=R"=====( <!DOCTYPE html><html>
  <head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">
  <link rel=\"icon\" href=\"data:,\">
  <style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}
  .button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;
  text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}
  .button2 {background-color: #77878A;}</style></head>
  <body><h1>NFC WiFi </h1>
  <p>Click register button and then place NFC tag over reader</p>
  <form action="/uid1rec" method="get">
    <button type="submit">Register</button><br></form>
  <p>Tag1 = ")=====";

const char MAIN_page2[]PROGMEM=R"=====("</p>
  <p>URL1 = ")=====";


const char MAIN_page3[]PROGMEM=R"=====("</p>
  <FORM METHOD="POST"action="/submitURL1">
    <input type="text" name="myText" value="Tag1 URL...">
  <input type="submit" value="Submit">
  </form>
  <p>Once you've registered the tag and enter a URL hit Run and the reader will be working</p>
  <form action="/run" method="get">
    <button type="submit">Run</button><br>  
  </body></html>)=====";

String WebPage =""; 

void spiffsWrite(String, String);
String spiffsRead(String);
void LED_Off();
void LED_Blue();
void LED_Red();
void LED_Green();
void url1submit();
void uid1record();
void UseURL1(String);
void UpdateWebPage();
void nfcread();
void setClock();
void setup()
{ 
  Serial.begin(115200);
  bool result = SPIFFS.begin();
  


  if ( ! SPIFFS.exists("/uid1str") ) {
    spiffsWrite("/uid1str", "Not Set");
  }
  uid1str = spiffsRead("/uid1str");

  if ( ! SPIFFS.exists("/url1str") ) {
    spiffsWrite("/url1str", "Not Set");
  }
  url1str = spiffsRead("/url1str");

  
  Serial.println("Hello!");

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
  //server.on("/",handlePostForm);
  WebPage= MAIN_page1+uid1str+MAIN_page2+url1str+MAIN_page3;

  server.on("/", [](){  
    Serial.println("You called root page");
    server.send(200,"text/html",WebPage);
    });      
  server.on("/run", nfcread);
  server.on("/submitURL1", [](){ 
    url1submit();
    UpdateWebPage();
    server.send(200,"text/html",WebPage);
    });
  server.on("/uid1rec", [](){ 
      uid1record();
      UpdateWebPage();
      server.send(200, "text/html", WebPage);
      });
  server.begin();
  WiFiManager wifiManager;
  wifiManager.autoConnect("NFC_WiFi");

  


  Serial.println("End Of Setup Loop");


}




void loop() {
  server.handleClient();
}


void url1submit()
{
 notice=server.arg("myText");
 Serial.println("Text Received, contents:");
 Serial.println(notice);
 if (notice != ""){
  url1str = notice;
  spiffsWrite("/url1str", notice);
 }
}

void uid1record(){
  LED_Blue();
  uint8_t success;
  uint8_t uidLength;  
  uid1str = "";

  bool waitforread = true;
  while(waitforread){
    success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

    if (success) {
      waitforread = false;
      Serial.println("Found an ISO14443A card");
      Serial.print("  UID Length: ");Serial.print(uidLength, DEC);Serial.println(" bytes");
      Serial.print("  UID Value: ");
      nfc.PrintHex(uid, uidLength);
      Serial.println("");

      for (int i = 0; i < uidLength;i++)
        uid1str = uid1str + uid[i];
      spiffsWrite("/uid1str", uid1str);

    }
  }
  
  LED_Off();
}
void UseURL1(String url1str)
{
  setClock();
    HTTPClient http;
    BearSSL::WiFiClientSecure *client = new BearSSL::WiFiClientSecure();
    BearSSL::CertStore certStore;
    int numCerts = certStore.initCertStore(&certs_idx, &certs_ar);
    client->setCertStore(&certStore);
    Serial.println("numCerts: "+numCerts);

  if (numCerts == 0) {
    Serial.printf("No certs found. Did you run certs-from-mozill.py and upload the SPIFFS directory before running?\n");
    return; // Can't connect to anything w/o certs!
  }
    Serial.println("calling: "+url1str);
    http.begin(dynamic_cast<WiFiClient&>(*client), url1str);
    int httpCode = http.GET();
    Serial.println("httpCode"+httpCode);

}

void UpdateWebPage(){
    WebPage = MAIN_page1+uid1str+MAIN_page2+url1str+MAIN_page3;
}

void nfcread(){
  while(true){
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;              // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
  
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

  if (success) {
    Serial.println("Found an ISO14443A card");
    Serial.print("  UID Length: ");Serial.print(uidLength, DEC);Serial.println(" bytes");
    Serial.print("  UID Value: ");
    nfc.PrintHex(uid, uidLength);
    Serial.println("");
    
    String readuid = "";
    for(int i=0;i<uidLength;i++)
    {
      readuid = readuid + uid[i];
    }
    
    if (uidLength == 4)
    {
      bool isMatch= false;


        if(readuid==uid1str)
        {
          isMatch = true;
        }
        else
        {
          isMatch =false;
        }

    
      if(isMatch == true)
      {
        LED_Green();
        UseURL1(url1str);
        isMatch == false;
        delay(5000);
        LED_Off();
      }
      if(isMatch == false)
      {
        LED_Red();
        isMatch == false;
        delay(3000);
        LED_Off();
      }
    
      Serial.println("Seems to be a Mifare Classic card (4 byte UID)");


      Serial.println("Trying to authenticate block 4 with default KEYA value");
      uint8_t keya[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

      success = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, 4, 0, keya);

      if (success)
      {
        Serial.println("Sector 1 (Blocks 4..7) has been authenticated");
        uint8_t nfcdata[16];

        success = nfc.mifareclassic_ReadDataBlock(4, nfcdata);
        
        if (success)
        {
          Serial.println("Reading Block 4:");
          nfc.PrintHexChar(nfcdata, 16);
          Serial.println("");

          delay(1000);
        }
        else
        {
          Serial.println("Ooops ... unable to read the requested block.  Try another key?");
        }
      }
      else
      {
        Serial.println("Ooops ... authentication failed: Try another key?");
      }
    }
    
    if (uidLength == 7)
    { 
      bool isMatch= false;

      String readuid = "";
      for(int i=0;i<uidLength;i++)
      {
        readuid = readuid + uid[i];
      }
      if(readuid==uid1str)
      {
        isMatch = true;
      }
      else
      {
        isMatch =false;
      }

      if(isMatch == true)
      {
        LED_Green();
        UseURL1(url1str);

        isMatch == false;
        delay(5000);
        LED_Off();
      }
      if(isMatch == false)
      {
        LED_Red();
        isMatch == false;
        delay(3000);
        LED_Off();
      }
      Serial.println("Seems to be a Mifare Ultralight tag (7 byte UID)");

      Serial.println("Reading page 4");
      uint8_t nfcdata[32];
      success = nfc.mifareultralight_ReadPage (4, nfcdata);
      if (success)
      {
        nfc.PrintHexChar(nfcdata, 4);
        Serial.println("");

        delay(1000);
      }
      else
      {
        Serial.println("Ooops ... unable to read the requested page!?");
      }
    }
  }

}
}

void spiffsWrite(String path, String contents) {
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
  ledStrip.sendColor(0,0,255,1);
  ledStrip.endFrame(1); 
}
void LED_Green()
{
  ledStrip.startFrame();
  ledStrip.sendColor(0,255,0,1);
  ledStrip.endFrame(1); 
}
void LED_Red()
{
  ledStrip.startFrame();
  ledStrip.sendColor(255,0,0,1);
  ledStrip.endFrame(1); 
}
void LED_Off()
{
  ledStrip.startFrame();
  ledStrip.sendColor(0,0,0,1);
  ledStrip.endFrame(1); 
}

void setClock() {
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
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