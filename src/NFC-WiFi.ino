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
//#include <IFTTTMaker.h>

ESP8266WebServer server(80);

// const byte url1size = 500;
// struct {
// 	uint url1len = 1;
// 	char url1[url1size] = "";
// 	uint uid1len = 1;
// 	uint8_t uid1[7] = "";

// } data;



const uint8_t clockPin = 4;
const uint8_t dataPin = 5;

#define PN532_SCK  (16)
#define PN532_MOSI (12)
#define PN532_SS   (13)
#define PN532_MISO (14)

uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 }; 
String uid1str = "Not Set";
String url1str = "Not Set";
Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);
APA102<dataPin, clockPin> ledStrip;

const uint16_t ledCount = 1;
const uint8_t brightness = 1;

String notice;

	


const char MAIN_page1[]PROGMEM=R"=====(	<!DOCTYPE html><html>
	<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">
	<link rel=\"icon\" href=\"data:,\">
	<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}
	.button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;
	text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}
	.button2 {background-color: #77878A;}</style></head>
	<body><h1>NFC WiFi </h1>
	<p>Click register button and then place NFC tag over reader</p>
	<form action="/record1" method="get">
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
 	server.on("/record1", [](){ 
   		Record1();
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

void printtest(){
	Serial.println("madeit!!!");
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




void Record1(){
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

    WiFiClient client;

    HTTPClient http;

    Serial.print("[HTTP] begin...\n");
    if (http.begin(client, url1str)) {  // HTTP


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

	UpdateWebPage();
}

void UpdateWebPage(){
	 	WebPage = MAIN_page1+uid1str+MAIN_page2+url1str+MAIN_page3;
}

void nfcread(){
	while(true){
	uint8_t success;
	uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };	// Buffer to store the returned UID
	uint8_t uidLength;							// Length of the UID (4 or 7 bytes depending on ISO14443A card type)
	
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