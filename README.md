# OS-RS485 - Alpha
OpenSteering-RS485 Data logger

Pins must be defined in rs485.h if you are using the RAK5802 you should set your pins as follows.

#define RS485_DEFAULT_DE_PIN (0xff)

#define RS485_DEFAULT_RE_PIN (0xff)

#define SERIAL_PORT_HARDWARE Serial1



# MQTT Downlink Config commands

To start rs485 measurements, with a single ComWinTop THC-S rs485 Soil sensor, send the following command.

2+01+03+00+00+00+03+05+CB

You can send these via MQTT downlink to the following sub
  
    MQTT_USER/MQTT_ID/config
    example: r4wk/test/config

This is all set in the mqtt_config.h

# Hardware needed

You'll want a RAK baseboard and RAK11200 core
- [RAK19007+RAK11200 KIT](https://rakwireless.kckb.st/57a05b8f) (Make sure to pick 19007+11200 in the drop down)
- [BASE BOARDS](https://rakwireless.kckb.st/e0a81f2e)
- [RAK11200](https://rakwireless.kckb.st/797d9c85)

And then you will want the RS485 Module: RAK5802

If you want to use the [SD card module](https://www.adafruit.com/product/4682), you will want to use the DUAL IO board ( [RAK19001](https://rakwireless.kckb.st/e5bcf28c) )

# How to flash
1. https://docs.rakwireless.com/Product-Categories/WisBlock/RAK11200/Quickstart/#install-platformio
2. Clone this repo to a local folder
3. Open cloned folder in VSCode+PlatformIO (from above)
4. Press the PlatformIO: Upload button (or however you want to build/flash)

# Support
If you want to support, use one of the referral links above to purchase your RAK hardware. OR just use the referral code
- [RAK Wireless Store](https://rakwireless.kckb.st/ace5fdc3) 8% off code: WGC279
  
You won't see the discount until you check out
