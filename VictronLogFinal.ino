// VictronLogFinal - Logging from Victron MPPT Solar Charge Controller
//
// Author:    Jurgen G Schmidt (www.jgscraft.com)
//            https://www.instructables.com/member/jgschmidt/instructables/
//
// Derived from Henk: https://www.romlea.nl/Arduino%20MPPT/Page.htm and https://youtu.be/w-9kYkSuCwc
//
// Method:    Data streams from MPPT as label and value pairs.
//            A loop reads these and updates variables
//            Periodically these variables are formatted into a csv string to be written to the log file on SD
//
// Hardware:  Arduino Mega2560 R3 - uses Serial and Serial1 (Pin 19)
//            Adafruit Datalogger Shield with battery and SD card
//            Victron SmartSolar MPPT 100/20 - pin1 GND, pin3 TX (no power or RX) use twisted pair for distance
//
// Software:  Arduino IDE 2.3.2
//            Arduino SD library
//            Adafruit RTClib
//
// 240517 - (JGS) begin testing just reading data from MPPT, then incorporate routines from my sketch RTC_SD_Testing
//        - reading data, time and SD logging work!!!
//
#include <RTClib.h>
#include <SPI.h>
#include <SD.h>

RTC_PCF8523   rtc;      // clock on shield, change if using different clock chip
File          myFile;

#define   FILENAME  "solar2.csv"
const int chipSelect = 10;    // CS pin for logger shield

#define LOGDELAY    20000L    // logging interval in milliseconds

String  label, val;       // holds read data, most gets tossed

int   vBattery, aBattery; // battery voltage in mV and current in mA
int   vPanel, pPanel;     // panel voltage im mV and power in watts
int   mpptCS;             // operating state
char  strLoad[5];         // load "ON" or "OFF"
int   aLoad;              // load current in mA
int   mpptError;          // error code

char  strLog[80];         // log record to wrie to SD

uint32_t  millisPrevious; // used to decide when to write a rrecord to SD
uint32_t  millisCurrent;

void setup() {

  strcpy(strLoad,"abc");

  Serial.begin(115200);         // goes to Arduino IDE serial monitor
  Serial1.begin(19200);         // from MPPT pin 3 to Mega pin 19

  // I can find out what's on a board by just plugging it in
  Serial.print(__DATE__); Serial.print("  "); Serial.println(__FILE__); // I can find out what's on a board by just plugging it in

  millisPrevious = millis();

  //------------ Clock setup ------------
  // Start the clock...
  if (! rtc.begin()) {
    Serial.println(F("Couldn't find RTC"));
    Serial.flush();
    while (1) delay(10);
  }

  //------------ Check if we need to reset the time...
  if (! rtc.initialized() || rtc.lostPower()) {
    Serial.println("RTC is NOT initialized, let's set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  rtc.start();    // clear the stop bit

  //------------ SD Card Setup ------------
  Serial.print(F("Initializing SD card..."));
  if (!SD.begin( chipSelect )) {
    Serial.println(F("initialization failed!"));
    while (1);
  }
  Serial.println(F("initialization done."));
  myFile = SD.open(FILENAME, FILE_WRITE);       // Open file for writing. Adds to end if it exists.
}

// Each run through the loop will find only one label/value pair, so it takes multiple
// loops to update all the desired variables
void loop() {
  if (Serial1.available()) {
    label = Serial1.readStringUntil('\t');                // this is the actual line that reads the label from the MPPT controller
    val = Serial1.readStringUntil('\r\r\n');              // this is the line that reads the value of the label
        
    if (label == "V") {                     // we found battery voltage in mV
      vBattery = val.toInt();               // update the current value
    }
    else if (label == "I") {                // we found battery mA 
      aBattery = val.toInt();    
    }
    else if (label == "VPV") {              // we found panel voltage mV 
      vPanel = val.toInt();    
    }
    else if (label == "PPV") {              // we found panel power in Watts
      pPanel = val.toInt();    
    }
    else if (label == "CS") {               // we found CS charge state
      mpptCS = val.toInt();    
    }
    else if (label == "LOAD") {             // we found LOAD - ON or OFF
      strcpy(strLoad, val.c_str());   
    }
    else if (label == "IL") {               // we found LOAD power in mA
      aLoad = val.toInt();    
    }
    else if (label == "ERR") {              // we found error code
      mpptError = val.toInt();    
    }
  } // end of reading Serial1

  // check to see if it's time to write a log record
  millisCurrent = millis();
  if(millisCurrent > millisPrevious+LOGDELAY){
    // time to do logging stuff...

    DateTime now = rtc.now();         // get date and time

    // build the csv log record...
    sprintf(strLog,"%d,%d,%d,%d,%d,%d,\"MPPT\",%d,%d,%d,%d,%d,\"%s\",%d,%d\n",
    now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second(),
    vBattery, aBattery, vPanel, pPanel, mpptCS,strLoad,aLoad,mpptError);
  
    Serial.println(strLog);

    myFile.print(strLog);     // send record to SD
    myFile.flush();           // make sure it's actually written since we never close the file

    millisPrevious = millisCurrent;     // reset timer

  } // end of logging

} // end of loop

//------ end of VictronLogFinal.ino ------//