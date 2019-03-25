// NFC-WiFi board simple program 2018 Matt Varian
// https://github.com/thematthewknot/NFC-WiFi
// released under the GPLv3 license.

#include <APA102.h>
#include <Adafruit_PN532.h>
#include <Wire.h>
#include <SPI.h>
#include <WiFiManager.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <EEPROM.h>
ESP8266WebServer server(80);

String header;

struct {
	uint val =0;
	char url1[500] = "";
	uint8_t uid1[7] = "";
} data;

const uint8_t clockPin = 4;
const uint8_t dataPin = 5;

#define PN532_SCK  (16)
#define PN532_MOSI (12)
#define PN532_SS   (13)
#define PN532_MISO (14)

static bool GoToRun = false;
uint8_t uid1[] = { 0, 0, 0, 0, 0, 0, 0 }; 
String uid1str = "";
String url1str = "";
Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);
APA102<dataPin, clockPin> ledStrip;

const uint16_t ledCount = 1;
const uint8_t brightness = 1;

String webPage,notice;




	
const char MAIN_page[]PROGMEM=R"=====(	<!DOCTYPE html><html>
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
	<p>Tag1 = "++"</p>
	<p>URL1 = <iframe name="URL1" width="10" height="10" frameBorder="0"></iframe></p>
	<FORM METHOD="POST"action="/submitURL1">
    <input type="text" name="myText" value="Tag1 URL...">
	<input type="submit" value="Submit">
	</form>
	<p>Once you've registered the tag and enter a URL hit Run and the reader will be working</p>
	<form action="/run" method="get">
  	<button type="submit">Run</button><br>
	</body></html>)=====";




void setup()
{	
	Serial.begin(115200);
	
	uint addr = 0;
	EEPROM.begin(512);
	EEPROM.get(addr,data);
	EEPROM.put(addr,data);
	EEPROM.commit();  
	EEPROM.get(addr,data);
	for (int i=0;i<7;i++)
	{
		uid1str = uid1str + data.uid1[i];
	}
	for (int i=0;i<500;i++)
	{
		url1str = url1str + data.url1[i];
	}



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
 	server.on("/", handleRoot);      
 	server.on("/record1", Record1);
 	server.on("/submitURL1", url1submit);
 	server.on("/run", nfcread );
	WiFiManager wifiManager;
	wifiManager.autoConnect("NFC_WiFi");
	server.begin();
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
 	saveURL(url1str);
 }
 server.send(200,"text/html","url1str");
}

void handleRoot() {
 Serial.println("You called root page");
 String s = MAIN_page; //Read HTML contents
 server.send(200, "text/html", s); //Send web page
}


void website(WiFiClient client)
{
		Serial.println("New Client.");
		String currentLine = "";
		while (client.connected()) {
		  if (client.available()) {
			char c = client.read();
			Serial.write(c);
			header += c;
			if (c == '\n') {

				if (currentLine.length() == 0) {
				client.println("HTTP/1.1 200 OK");
				client.println("Content-type:text/html");
				client.println("Connection: close");
				client.println();
				if (header.indexOf("GET /run") >= 0) {
					Serial.println("Entering run");
					GoToRun = true;
					nfcread();
				}
				else if(header.indexOf("GET /record1") >=0) {
					Serial.println("entering record1\n");
					Record1();
				}
				else if(header.indexOf("GET /submitURL1") >=0) {
					Serial.println("entering submitURL1\n");
					Serial.println(header[1]);
					url1str = header.substring(26);
	
					//url1str = url1str.substring(0,url1str.indexOf('HTTP/1.1')-2);
					int tempPos = int(url1str.indexOf('%0D'))-2;
					Serial.println("split pos is: "+tempPos);
					//url1str.remove(tempPos);

					saveURL(url1str);
				}
				client.println("<!DOCTYPE html><html>");
				client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
				client.println("<link rel=\"icon\" href=\"data:,\">");
				client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
				client.println(".button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;");
				client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
				client.println(".button2 {background-color: #77878A;}</style></head>");
				client.println("<body><h1>NFC WiFi </h1>");
				client.println("<p>Click register button and then place NFC tag over reader</p>");
				client.println("<p><a href=\"/record1\"><button class=\"button\">Register</button></a></p>");
				client.println("<p>Tag1 = "+uid1str+"</p>");
				client.println("<p>URL1 = "+url1str+"</p>");
				client.println("<form action=/submitURL1>");
				client.println("<textarea name=myTextBox cols=50rows=1>");
				client.println("Enter URL to trigger for Tag 1");
				client.println("</textarea>");
				client.println("<br />");
				client.println("<input type=submit />");
				client.println("</form>");
				client.println("<p>Once you've registered the tag and enter a URL hit Run and the reader will be working</p>");
				client.println("<p><a href=\"/run\"><button class=\"button\">Run</button></a></p>");
				client.println("</body></html>");
				client.println();
				break;
				} else { 
					currentLine = "";
				}
				} else if (c != '\r') {
				  currentLine += c; 
				}
			}
		}
		header = "";
		client.stop();
		Serial.println("Client disconnected.");
		Serial.println("");
}

void Record1(){
	uint8_t success;
	uint8_t uidLength;	
	uid1str = "";

	bool waitforread = true;
	while(waitforread){
		success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, data.uid1, &uidLength);

		if (success) {
			waitforread = false;
			Serial.println("Found an ISO14443A card");
			Serial.print("  UID Length: ");Serial.print(uidLength, DEC);Serial.println(" bytes");
			Serial.print("  UID Value: ");
			nfc.PrintHex(data.uid1, uidLength);
			Serial.println("");
			for (int i=0;i<uidLength;i++)
			{
				uid1str = uid1str + data.uid1[i];
			}
			
			EEPROM.put(0,data);
			EEPROM.commit();

		}
	}

}

void saveURL(String url1str){
			Serial.println("Here the URL1 being saved:"+url1str);
 			server.send(200, "text/html", url1str); //Send web page

			for (int i=0;i<url1str.length();i++)
			{ 
				data.url1[i] = url1str[i] ;
			}
			for (int i=0;i<500;i++)
			{
			url1str = url1str + data.url1[i];
			}
			// for (int i = URL1.length(); i < 500; i++)
			// {
			// 	data.url1[i] = 0;
			// }
			EEPROM.put(0,data);
			EEPROM.commit();
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


}


void nfcread(){
	uint8_t success;
	uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };	// Buffer to store the returned UID
	uint8_t uidLength;							// Length of the UID (4 or 7 bytes depending on ISO14443A card type)
	

	success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

	if (success) {
		Serial.println("Found an ISO14443A card");
		Serial.print("  UID Length: ");Serial.print(uidLength, DEC);Serial.println(" bytes");
		Serial.print("  UID Value: ");
		nfc.PrintHex(data.uid1, uidLength);
		Serial.println("");
		
		
		if (uidLength == 4)
		{
			bool isMatch= false;
			for(int j = 0; j <4; j++)
			{	
				if(uid[j]==data.uid1[j])
				{
					isMatch = true;
				}
				else
				{
					isMatch =false;
					break;
				}

			}
			if(isMatch == true)
			{
				LED_Green();
				UseURL1(url1str);
				isMatch == false;
				delay(1000);
			}
			if(isMatch == false)
			{
				LED_Red();
				isMatch == false;
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
			for(int j = 0; j <7; j++)
			{	
				if(uid[j]==data.uid1[j])
				{
					isMatch = true;
				}
				else
				{
					isMatch =false;
					break;
				}
			}    
			if(isMatch == true)
			{
				LED_Green();
				UseURL1(url1str);

				isMatch == false;
				delay(1000);
			}
			if(isMatch == false)
			{
				LED_Red();
				isMatch == false;
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
