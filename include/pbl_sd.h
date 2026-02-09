#include <SPI.h>
#include <SD.h>
#include "time.h"

File myFile;
#ifndef SCK_PIN
#define SCK_PIN 5
#endif
#ifndef MISO_PIN
#define MISO_PIN 7
#endif
#ifndef MOSI_PIN
#define MOSI_PIN 6
#endif
#ifndef CS_PIN
#define CS_PIN 4
#endif 

bool setupSD() {
    /*
    Set up SD card module with SPI communication 
        SCK: 5
        MISO: 7
        MOSI: 6
        CS: 4
    */
    SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);  // SCK = 5, MISO = 7, MOSI = 6, CS = 4
    SD.end();
    if (!SD.begin(CS_PIN)) {
        return false;
    }
    return true;
}

bool printSD(String data) {
    if (!SD.begin(CS_PIN)) {
        return false;
    } else {
        try{
            myFile = SD.open("/test.txt", FILE_APPEND);  // Open file for writing
            if (myFile) {
                // Write data to file
                myFile.println(String(data));
                myFile.close();
                return true;
            } else {
                return false;
            }
        } catch (...) {
            return false;
        }
    } 
}

