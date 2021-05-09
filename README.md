# retr-o-phone-v2
How to revive an old retro phone and turn it in to a functioning "mobile" phone with a battery and sim card, using an Adafruit Feather 32u4 FONA

![image of phone](/doc/phone.jpg)

# Equipment
These are the components i used for the project

- Old rotary dial phone from 'Elektrisk Bureau', as shown in the image above
- Adafruit Feather 32u4 FONA (https://www.adafruit.com/product/3027)
- LiPo battery (https://www.adafruit.com/product/258)
- Antenna (https://www.adafruit.com/product/3237)
- Twin sim card, with the same phone number as my normal mobile phone, such that i can use the rotary phone in parallel
- Microphone (https://www.adafruit.com/product/1064)
- Speaker (https://www.adafruit.com/product/1890)
- Buzzer (https://www.adafruit.com/product/1536)
- Battery indicator led (https://www.adafruit.com/product/159)
- Switch for hanging up when conversations get boring (https://www.adafruit.com/product/819)
- Bread board (https://www.adafruit.com/product/64)
- Couple of breadboard wires
- pin headers
- USB-C panel mount cable for charging and programming (https://www.adafruit.com/product/4056)

The whole thing should cost about $100

# Schematic

![schematic](/doc/schematic.png)

# How to get started

Follow the guide from Adafruit on how to set up the feather FONA board. Link is in the Resources section below.

Connect everything as shown in the schematic

Download this project from github and upload it to your FONA

You will also have to set your SIM card PIN number, and the default country code in the source code, here:
![simpin](/doc/simpin.png)

Connect to power, and call one of your friends to ask if they would like to hang out, like you used to do after school, 20 years ago.



# Arduino libraries

You will need the following libraries. They can be downloaded from your Arduino IDE.

- Adafruit_FONA.h
- SoftwareSerial

# Resources

- https://learn.adafruit.com/adafruit-feather-32u4-fona?view=all
- https://www.finn.no/bap/forsale/search.html?search_type=SEARCH_ID_BAP_ALL&q=televerket%20telefon