![Here's the board](https://i.imgur.com/lpjVlrc.jpg)

# What is NFC-WiFi?
It is a NFC reader with WiFi!  
This project joins a NFC reader(PN532) to a WeMos D1 Mini to allow for WiFi access.  
What can you do with this?
* Toggle a IFTTT event with a NFC tag
* Integrate into home automation systems
* Send notification when scanned
* And much more

...something to note we're not 100% there yet, the hardware is done, now time to catch the
software upto make those things happen.  


# Where do I get one?
Want one? Sign up to preorder one now: [Preorder](https://goo.gl/forms/MPRosQnBkixg8Vhj2)  
$20+shipping gets you a built/tested/working board, USB cable, Enclosure, All you need to get going.  

I have about 2 dozen made and ready to ship currently, but the software isn't done yet.  

If you want one and aren't afraid of getting dirty in the code let me know in the comments of the sign up and I'll gladly send you one.  

If you just want it to work sign up for the waiting list and once we're up and going I'll reach out to you.  

# Hardware used
* PN532 Board
* WeMos D1 Mini 4M
* custom PCB to join the two with addressable RGB LED
* 3D printed Enclosure

# Pinout Notes
## Led (APA102)
* data - 5
* clock - 4

## D1 Mini (WeMos 4M)
* sck - 16 
* miso - 14
* mosi - 12
* ss - 13

# Enclosure 
The enclosure is 1.8in x 1.85in x 0.8in(46mm x 47mm x 20mm) 3D printed with PETG so should hold up to outdoors to some extent however if it's going to be outdoors you'll want to seal up the hole where the USB connects.  

There are also some diviots to ease in mounting via screws, or you can use double sided tape to attach it to a surface.  

There is a lid that can slide off to allow you to remove the boards from inside. The hole is designed for the USB cable I send with board/enclosures when purchased but should fit most micro-USB cables.  
![Enclosure](https://i.imgur.com/oRg27Vw.png)