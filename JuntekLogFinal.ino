// JuntekLogFinal - Looging frum JUNTEK Battery Monitor
//
// Author:    Jurgen G Schmidt (www.jgscraft.com)
//            https://www.instructables.com/member/jgschmidt/instructables/
//
// Method:    Data streams from JUNTEK
//            Detect :r50 in stream
//            Repeated parseInts() collect numbers from the stream
//            Verify checksum
//            Update variables as we find them
//            Log to SD periodically
// 
// Hardware:  Arduino Mega2560 R3 - uses Serial and Serial2 (pin 17)
//            Adafruit Datalogger Shield with battery and SD card
//            JUNTEK (or WonVon) KG110F Battery monitor
//            - Turn off the timing option, it messes up the current checksum calculation 
//            - could probably fix this by using uint32_t
//
// Software:  Arduino IDE 2.3.2
//            Arduino SD library
//            Adafruit RTClib
//
// 240518 - (JGS) Find best strategy for reading data
//        - NOTE: Arduino sprintf() does not support float (%f) - use dtostrf()
//          dtostrf(float, min-width, decimal places, destination buffer);
//          be sure to (float) cast in any float calculations:  floatnum = (float)intnum/100;
//
// 240521 - Fix weird timing issue to read successive characters...
//          ...try readBytesUntil() and eliminate an if() structure
//          Works!!! Keep this version.
//
#include <RTClib.h>
#include <SPI.h>
#include <SD.h>

RTC_PCF8523   rtc;      // clock on shield, change if using different clock chip
File          myFile;

#define   FILENAME  "solar-jt.csv"
const int chipSelect = 10;    // CS pin for SD on logger shield

#define LOGDELAY    60000L    // logging interval in milliseconds

// Received variables from the :r50 record
// We keep all values so we can calculate a checksum
int   bmAddress;      // Device address
int   bmCheck;        // Checksum
int   bmVolts;        // Battery voltage, divide by 100 
int   bmAmps;         // Amps flowing, divide by 100      
int   bmCapacity;     // remaining Ah, divide by 1000     
int   bmCumCap;       // ? Ah, divide by 1000             
int   bmWattHours;    // 
int   bmRunTime;      // seconds monitor running   
int   bmTemp;         // Temp in C, subtract 100
int   bmWatts;        // divide by 100
int   bmStatus;       // Status 0=on, 255=off, some others
int   bmDir;          // flow direction 0=charging 1=discharging
int   bmLife;         // remaining battery life, minutes
int   bmResistance;   // battery internal resistance

unsigned int   chkSum;         // calculated checksum

char  inChar;         // incoming single character
char  inRead[8];       // buffer for readBytesUntil()
char  strBuf[80];     // space to build our logging record
char  strLog[80];     // spare
float tmpFloat;       // used for int to float conversion
char  buf1[12], buf2[12], buf3[12];  // used for float to string for sprintf()

uint32_t  millisPrevious; // used to decide when to write a rrecord to SD
uint32_t  millisCurrent;

void setup()  
{
  Serial.begin(115200);
  Serial2.begin(115200);

  // I can find out what's on a board by just plugging it in
  Serial.print(__DATE__); Serial.print("  "); Serial.println(__FILE__);

  millisPrevious = millis();

  //------------ Clock setup ------------
  // Start the clock...
  if (! rtc.begin()) {
    Serial.println(F("Couldn't find RTC"));
    Serial.flush();
    while (1);
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

void loop() 
{
  if (Serial2.available()) {
    // can take a few rounds to synch up...
    inChar = Serial2.read();                    // look for :
    if(inChar==':') {

      Serial2.readBytesUntil(',', inRead, 3);   // look for r50
      if( strncmp("r50", inRead, 3) == 0) {
      
        bmAddress = Serial2.parseInt();
        bmCheck = Serial2.parseInt();
        bmVolts = Serial2.parseInt();
        bmAmps = Serial2.parseInt();
        bmCapacity = Serial2.parseInt();
        bmCumCap = Serial2.parseInt();
        bmWattHours = Serial2.parseInt();
        bmRunTime = Serial2.parseInt();
        bmTemp = Serial2.parseInt();
        bmWatts = Serial2.parseInt();
        bmStatus = Serial2.parseInt();
        bmDir = Serial2.parseInt();
        bmLife = Serial2.parseInt();
        bmResistance = Serial2.parseInt();

        // all variables are added up to calculate checksumm
        chkSum = bmVolts+bmAmps+bmCapacity+bmCumCap+bmWattHours+bmRunTime+bmTemp;
        chkSum += bmWatts+bmStatus+bmDir+bmLife+bmResistance;
        chkSum = chkSum%255+1;

        if(bmCheck!=chkSum) { 
          Serial.println("---------------- checksum error-----------------"); 
          sprintf(strLog,"ALL: |%d| %d %d %d %d %d %d %d %d %d %d %d %d %d",
                chkSum,bmCheck,bmVolts,bmAmps,bmCapacity,bmCumCap,bmWattHours,bmRunTime,bmTemp,bmWatts,bmStatus,bmDir,bmLife,bmResistance);
          Serial.println(strLog);      
        } else {
          // Checksum is OK - Update some readings

          // Arduino sprintf() does not do floats so need to use dtostrf() to make strings of floats
          tmpFloat = (float)bmVolts/100;
          dtostrf(tmpFloat, 5, 2, buf1);

          tmpFloat = (float)bmAmps/100;
          dtostrf(tmpFloat, 5, 2, buf2);

          tmpFloat = (float)bmCapacity/1000;
          dtostrf(tmpFloat, 5, 2, buf3);

          /*
          sprintf(strBuf,"JTDATA: %sV %sA %sAh %dC %dS %dDIR", 
                  buf1, buf2, buf3, bmTemp-100, bmStatus, bmDir);
          Serial.println(strBuf);    
          */
        }   // end of checksum verification
      }     // end of 'r50' detection 
    }       // end of colon detection
  }         // end of reading JUNTEK (Serial2)

  // check to see if it's time to write a log record
  millisCurrent = millis();
  if(millisCurrent > millisPrevious+LOGDELAY){
    // time to do logging stuff...

    DateTime now = rtc.now();         // get date and time

    // build the csv log record...
    sprintf(strLog,"%d,%d,%d,%d,%d,%d,\"JT\",%s,%s,%s,%d,%d,%d\n",
    now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second(),
    buf1, buf2, buf3, bmTemp-100, bmStatus,bmDir);
    Serial.println(strLog);

    myFile.print(strLog);     // send record to SD
    myFile.flush();           // make sure it's actually written since we never close the file

    millisPrevious = millisCurrent;     // reset timer
  } // end of logging

} // end of loop

//------ end of JuntekLogFinal.ino ------//