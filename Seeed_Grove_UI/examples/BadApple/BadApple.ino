/*
  SD card read/write

 This example shows how to read and write data to and from an SD card file
 The circuit:
 * SD card attached to SPI bus as follows:
 ** MOSI - pin 11
 ** MISO - pin 12
 ** CLK - pin 13
 ** CS - pin 4 (for MKRZero SD: SDCARD_SS_PIN)

 created   Nov 2010
 by David A. Mellis
 modified 9 Apr 2012
 by Tom Igoe

 This example code is in the public domain.

 */

#ifdef KENDRYTE_K210
  #include <SPIClass.h>
#else
  #include <SPI.h>
#endif

#include "Seeed_FS.h"
#include "SD/Seeed_SD.h"
#include "TFT_eSPI.h"

#define SERIAL          Serial
#define SDCARD_SPI      SPI1
#define SDCARD_MISO_PIN PIN_SPI1_MISO
#define SDCARD_MOSI_PIN PIN_SPI1_MOSI
#define SDCARD_SCK_PIN  PIN_SPI1_SCK
#define SDCARD_SS_PIN   PIN_SPI1_SS

constexpr uint32_t screen_width = 320;
constexpr uint32_t screen_height = 240;
File      file;
TFT_eSPI  tft;
uint8_t   img[screen_width][screen_height];
uint8_t   raw[screen_width * screen_height / 8];
uint8_t * buf = (uint8_t *)img;

void unpack(){
    for (int j = 0, k = 0; j < sizeof(raw); j++){
        for (int t = k + 8; k < t; k++){
            buf[k] = raw[j] & 0x80 ? 0xff : 0x00;
            raw[j] <<= 1;
        }
    }  
}

void setup() {
    // open SERIAL communications and wait for port to open:
    SERIAL.begin(115200);
    
    // wait for SERIAL port to connect. Needed for native USB port only
    while (!SERIAL) {
        ;
    }

    SERIAL.println("initializing SD card...");
    
    if (!SD.begin(SDCARD_SS_PIN, SDCARD_SPI)) {
        SERIAL.println("initialization failed!");
        while (1);
    }

    tft.init();
    tft.setRotation(2);
    SERIAL.println("initialization done.");
    file = SD.open("bad-apple20fps.raw", FILE_READ);
}

void loop() {
    // open the file. note that only one file can be open at a time,
    // so you have to close this one before opening another.
    // read from the file until there's nothing else in it:
    if (file) {
        while (file.available()){
            file.read(raw, sizeof(raw));
            unpack();
            tft.pushImage(0, 0, tft.width(), tft.height(), buf);
        }
        file.close();
        file = SD.open("bad-apple20fps.raw", FILE_READ);
    } 
    else {
        // if the file didn't open, print an error:
        SERIAL.println("can't open bad-apple20fps.raw");
    }
}
