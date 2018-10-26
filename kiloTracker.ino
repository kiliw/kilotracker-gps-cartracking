/*
 * _  ___ _    _______             _             
 *| |/ (_) |  |__   __|           | |            
 *| ' / _| | ___ | |_ __ __ _  ___| | _____ _ __ 
 *|  < | | |/ _ \| | '__/ _` |/ __| |/ / _ \ '__|
 *| . \| | | (_) | | | | (_| | (__|   <  __/ |   
 *|_|\_\_|_|\___/|_|_|  \__,_|\___|_|\_\___|_| 
 *
 *
 * GPS/GSM Alarm for Campervans
 * 
 * 
 * Features:
 * - Low Power mode after defined amount of time
 * - SMS interrupt, wake up from idle and timer restart
 * - set desired amount of time (min) to enter low power
 * - PIR interrupt, wake up arduino, activate alarm, only activate again after 1min
 * - Send SMS with PIR to toggle PIR Sensor On/Off
 *
 */

//---------------------------
//++++++++ INITIALIZING ++++++++
//---------------------------
#include "Adafruit_FONA.h"
#include <LowPower.h>
#include <SoftwareSerial.h>
#include "number_config.h" //include the information about telefonenumber and name
#include "tools.h"         //external functions

//---------------------------
//++++++++ SETUP ++++++++
//---------------------------
void setup()
{

  pinMode(gsmPower, OUTPUT);
  pinMode(PIRoutPin, OUTPUT);
  pinMode(ledPin, OUTPUT);

  digitalWrite(PIRoutPin, LOW); //set 5V for PIR
  digitalWrite(gsmPower, LOW);  // set HIGH to trigger ON/OFF Sim808

  attachInterrupt(digitalPinToInterrupt(PIRinterruptPin), PIRtimerReset, RISING);
  attachInterrupt(digitalPinToInterrupt(RIinterruptPin), RItimerReset, RISING);

  simPowerOn(); //Power On GSM modul

  simStart(); //Start the GSM modul
}

//---------------------------
//++++++++ LOOP ++++++++
//---------------------------
void loop()
{

  while (timer < 120000)
  {

    //Timer
    currentMillis = millis();
    timer = timer + (currentMillis - previousMillis);
    timer2 = timer2 + (currentMillis - previousMillis);
    previousMillis = currentMillis;

    getSentSMS(); //main function to receive SMS

    if (pirMotion == true)
    {
      digitalWrite(ledPin, HIGH);
      delay(2000);
      digitalWrite(ledPin, LOW);
      if (timer2 >= 60000)
      {
        caseCounter = 1; //Set alarm case
      }
      else
      {
        pirMotion = false; //set to false to only accept motion after 1 min
      }
    }

    //Execute different actions depending on SMS you send
    switch (caseCounter)
    {
    case 1:
      //PIR was triggerd, activate Alarm!

      //uncomment, just for testing if PIR is working
      //simPowerOn(); //uncomment when functional
      //simStart();

      fona.sendSMS(mobileNumber, "Duci braucht Hilfe!!!");
      enableGPS();
      getGPS(); //!!! here occures arduino restart

      pirMotion = false;
      caseCounter = 0;
      timer = 0;
      timer2 = 0;
      break;
    case 2:
      //Message with GPSON was received!
      enableGPS();
      caseCounter = 0;
      timer = 0;
      break;
    case 3:
      //Message with GPSOF was received!
      fona.enableGPS(false);
      caseCounter = 0;
      timer = 0;
      break;
    case 4:
      //Message with GPSLO was received!
      getGPS();
      caseCounter = 0;
      timer = 0;
      break;
    case 5:
      //Message with GSMOF was received!
      simPowerOff();
      caseCounter = 0;
      timer = 0;
      break;
    case 6:
      //Message with GSMRE was received!
      simPowerOff();
      simPowerOn();
      simStart();

      caseCounter = 0;
      timer = 0;
      break;
    case 7:
      //Message with PIR was received!
      if (pirState == LOW)
      {
        digitalWrite(PIRoutPin, HIGH); //set 5V for PIR
        fona.sendSMS(mobileNumber, "PIR activated!");
        pirState = HIGH;
      }
      else
      {
        digitalWrite(PIRoutPin, LOW);
        fona.sendSMS(mobileNumber, "PIR deactivated!");
        pirState = LOW;
      }

      caseCounter = 0;
      timer = 0;
      break;
    } //End switch case
  }   //End while timer

  //Enter low power Mode
  timer2 = 0;
  sleepInd = true;
  fona.enableGPS(false); //switch GPS off
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);

} //End Loop
