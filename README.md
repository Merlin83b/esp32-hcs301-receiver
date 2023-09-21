# ESP32 HCS301/Keeloq receiver

## Overview

The [HCS301](https://www.microchip.com/en-us/product/hcs301) is a chip that runs the Keeloq system in remote key entry systems like garage doors and access gates.  This code is written for an ESP32 (I use it on a [TinyS3](https://esp32s3.com/tinys3.html) which has an ESP32-S3) to receive and interpret the signal from a remote.

I use a generic RXB6 superheterodyne 433MHz receiver ([this one](https://www.ebay.co.uk/itm/161662463558)) from eBay.  Plenty are available from all the usual places, and if you're more patient than me you can get them from [AliExpress](https://www.aliexpress.com/w/wholesale-rxb6.html) for less than £1/$1.

## Limitations

I'm not doing anything here to decode the encrypted part of the Keeloq transmission.  You can find other projects and details about that, but this code is just for receiving the transmission.

## Running the code

You'll need the ESP-IDF installed and configured, as detailed in [their instructions](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html).

Hook up your RXB6 module to ground, power (I use the TinyS3's 3.3V output, the module will accept 3-5.5V), and data.  The code is set to use pin 34, but that's configurable through a `#define` at the top of `main.c`.

With all that done, check the code out and build it.  You'll see details of remote transmissions on the console

## Thanks

I based the code on Manuel Schütze's post on his blog at https://www.manuelschutze.com/?p=333.  That has code for Arduinos which works great.