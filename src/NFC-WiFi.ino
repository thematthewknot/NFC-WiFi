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
#include <string.h>

WiFiServer server(80);

String header;

const uint8_t clockPin = 4;
const uint8_t dataPin = 5;

#define PN532_SCK  (16)
#define PN532_MOSI (12)
#define PN532_SS   (13)
#define PN532_MISO (14)

static bool GoToRun = false;
uint8_t uid1[] = { 0, 0, 0, 0, 0, 0, 0 }; 
String uidstr = "";
Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);
APA102<dataPin, clockPin> ledStrip;

const uint16_t ledCount = 1;
const uint8_t brightness = 1;

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


void setup() 
{
	Serial.begin(115200);
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

	WiFiManager wifiManager;
	wifiManager.autoConnect("NFC_WiFi");
	server.begin();
	Serial.println("End Of Setup Loop");
}




void loop() {  
	WiFiClient client = server.available();  
	if (client) {                            
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

				client.println("<!DOCTYPE html><html>");
				client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
				client.println("<link rel=\"icon\" href=\"data:,\">");
				client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
				client.println(".button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;");
				client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
				client.println(".button2 {background-color: #77878A;}</style></head>");
				client.println("<body><h1>NFC WiFi </h1>");
				client.println("<p>Place NFC tag over reader and click to register Tag 1 </p>");
				client.println("<p><a href=\"/record1\"><button class=\"button\">Tag 1</button></a></p>");
				client.println("<p>Tag1 = "+uidstr+"</p>");
				client.println("<form action=//www.html.am/html-codes/textboxes/submitted.cfm>");
				client.println("<textarea name=myTextBox cols=50rows=1>");
				client.println("Enter URL to trigger for Tag 1");
				client.println("</textarea>");
				client.println("<br />");
				client.println("<input type=submit />");
				client.println("</form>");
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
	  
	if(GoToRun){
		nfcread();
	    
	}
}
void Record1(){
	uint8_t success;
  	uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
    
	  // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
	  // 'uid' will be populated with the UID, and uidLength will indicate
	  // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
  	success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid1, &uidLength);
  
  	if (success) {
	    // Display some basic information about the card
	    Serial.println("Found an ISO14443A card");
	    Serial.print("  UID Length: ");Serial.print(uidLength, DEC);Serial.println(" bytes");
	    Serial.print("  UID Value: ");
	    nfc.PrintHex(uid1, uidLength);
	    Serial.println("");
	    for (int i=0;i<uidLength;i++)
	    {
	    	uidstr = uidstr + uid1[i];

	    }
	}

}






void nfcread(){
	uint8_t success;
  	uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  	uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
    
	  // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
	  // 'uid' will be populated with the UID, and uidLength will indicate
	  // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
  	success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
  
  	if (success) {
	    // Display some basic information about the card
	    Serial.println("Found an ISO14443A card");
	    Serial.print("  UID Length: ");Serial.print(uidLength, DEC);Serial.println(" bytes");
	    Serial.print("  UID Value: ");
	    nfc.PrintHex(uid, uidLength);
	    Serial.println("");
	    
	    
		if (uidLength == 4)
		{
		    bool isMatch= false;
		    for(int j = 0; j <4; j++)
		    {	
		    	if(uid[j]!=uid1[j])
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
		    	isMatch == false;
		    	delay(1000);
		    }
		    if(isMatch == false)
		    {
		    	LED_Red();
		    	isMatch == false;
		    }
		   
	      // We probably have a Mifare Classic card ... 
	      Serial.println("Seems to be a Mifare Classic card (4 byte UID)");
		  
	      // Now we need to try to authenticate it for read/write access
	      // Try with the factory default KeyA: 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF
	      Serial.println("Trying to authenticate block 4 with default KEYA value");
	      uint8_t keya[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
		  
		  // Start with block 4 (the first block of sector 1) since sector 0
		  // contains the manufacturer data and it's probably better just
		  // to leave it alone unless you know what you're doing
	      success = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, 4, 0, keya);
			  
			if (success)
			{
				Serial.println("Sector 1 (Blocks 4..7) has been authenticated");
		        uint8_t data[16];
				
		        // If you want to write something to block 4 to test with, uncomment
				// the following line and this text should be read back in a minute
		        //memcpy(data, (const uint8_t[]){ 'a', 'd', 'a', 'f', 'r', 'u', 'i', 't', '.', 'c', 'o', 'm', 0, 0, 0, 0 }, sizeof data);
		        // success = nfc.mifareclassic_WriteDataBlock (4, data);

		        // Try to read the contents of block 4
				success = nfc.mifareclassic_ReadDataBlock(4, data);
				
				if (success)
				{
				  // Data seems to have been read ... spit it out
					Serial.println("Reading Block 4:");
					nfc.PrintHexChar(data, 16);
					Serial.println("");

					// Wait a bit before reading the card again
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
		    	if(uid[j]!=uid1[j])
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
		    	isMatch == false;
		    	delay(1000);
		    }
		    if(isMatch == false)
		    {
		    	LED_Red();
		    	isMatch == false;
		    }
	      // We probably have a Mifare Ultralight card ...
	      Serial.println("Seems to be a Mifare Ultralight tag (7 byte UID)");
		  
	      // Try to read the first general-purpose user page (#4)
	      Serial.println("Reading page 4");
	      uint8_t data[32];
	      success = nfc.mifareultralight_ReadPage (4, data);
	      if (success)
	      {
	        // Data seems to have been read ... spit it out
	        nfc.PrintHexChar(data, 4);
	        Serial.println("");
			
	        // Wait a bit before reading the card again
	        delay(1000);
	      }
	      else
	      {
	        Serial.println("Ooops ... unable to read the requested page!?");
	      }
	    }
	  }

}

