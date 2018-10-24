/*
 * _  ___ _    _______             _             
 *| |/ (_) |  |__   __|           | |            
 *| ' / _| | ___ | |_ __ __ _  ___| | _____ _ __ 
 *|  < | | |/ _ \| | '__/ _` |/ __| |/ / _ \ '__|
 *| . \| | | (_) | | | | (_| | (__|   <  __/ |   
 *|_|\_\_|_|\___/|_|_|  \__,_|\___|_|\_\___|_| 
 *
 *
 * GPS/GSM Alarm for Duci
 * 
 * To Do:
 * 
 * Features:
 * - Low Power mode after defined amount of time
 * - SMS interrupt, wake up from idle and timer restart
 * - set desired amount of time (min) to enter low power
 * - PIR interrupt, wake up arduino, activate alarm, only activate again after 1min
 * - Send SMS with PIR to toggle PIR Sensor On/Off
 * 
 * Changed:
 *
 */

//---------------------------
//++++++++ INITIALIZING ++++++++
//--------------------------- 
#include "Adafruit_FONA.h"
#include <LowPower.h>
#include <SoftwareSerial.h>
#include "numberConfig.h" //include the information about telefonenumber and name


#define FONA_RX 8
#define FONA_TX 7
#define FONA_RST 4

SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);
SoftwareSerial *fonaSerial = &fonaSS;

// Use this for FONA 800 and 808s
Adafruit_FONA fona = Adafruit_FONA(FONA_RST);

// Defining Pins
#define gsmPower 5        // powering GSM modul
#define ledPin 13         // just for testing PIR
#define PIRoutPin 12      // Pin to power PIR
#define PIRinputPin 2     // Pin for read PIR
#define PIRinterruptPin 2 // Pin for interrupt
#define RIinterruptPin 3  // Pin which is connected to the RI pin of Sim808

unsigned long currentMillis;
unsigned long previousMillis = 0;
uint64_t timer = 0;
uint64_t timer2 = 0;

char fonaNotificationBuffer[64];      //for notifications from the FONA
char smsBuffer[25];
//char mobileNumber[]="+4912345678"; //mobile number should be defined in conifg.h

uint8_t readline(char *buff, uint8_t maxbuff, uint16_t timeout = 0);
float latitude, longitude, speed_kph, heading, speed_mph, altitude;

boolean pirMotion = false; //set true when PIR triggers interrupt
boolean sleepInd = false; //indicate if sleep mode was activated
boolean gps_success;
int pirState = 0;


int count1 = 0; //initialize counter to get 3D fix

// Set here different SMS controls
int caseCounter = 0;
char option_1[5] = {'G','P','S','O','N'};
char option_2[5] = {'G','P','S','O','F'};
char option_3[5] = {'G','P','S','L','O'};
char option_4[5] = {'G','S','M','O','F'};
char option_5[5] = {'G','S','M','R','E'};
char option_6[3] = {'P','I','R'};

//---------------------------
//++++++++ SETUP ++++++++
//---------------------------
void setup() {

  pinMode(gsmPower,OUTPUT);
  pinMode(PIRoutPin,OUTPUT);
  pinMode(ledPin,OUTPUT);

  digitalWrite(PIRoutPin, LOW); //set 5V for PIR
  digitalWrite(gsmPower,LOW); // set HIGH to trigger ON/OFF Sim808
  
  attachInterrupt(digitalPinToInterrupt(PIRinterruptPin), PIRtimerReset, RISING);
  attachInterrupt(digitalPinToInterrupt(RIinterruptPin), RItimerReset, RISING);
  
  simPowerOn(); //Power On GSM modul
  
  simStart();     //Start the GSM modul
}

//---------------------------
//++++++++ LOOP ++++++++
//---------------------------
void loop() {

   while (timer < 120000) {
    
    //Timer
    currentMillis = millis();
    timer = timer + (currentMillis - previousMillis);
    timer2 = timer2 + (currentMillis - previousMillis);
    previousMillis = currentMillis;

    ////////////////
    getSentSMS(); //main function to receive SMS
    ////////////////


    if (pirMotion == true) {
      digitalWrite(ledPin,HIGH);
      delay(2000);
      digitalWrite(ledPin,LOW);
      if (timer2 >= 60000 ) {
        caseCounter = 1; //Set alarm case
      } else {
      pirMotion = false; //set to false to only accept motion after 1 min
      }   
    } 


//Execute different actions depending on SMS you send
    switch(caseCounter) {
      case 1:
      //PIR was triggerd, activate Alarm!
      
      //uncomment, just for testing if PIR is working
      //simPowerOn(); //uncomment when functional
      //simStart();

      fona.sendSMS(mobileNumber, "Duci braucht Hilfe!!!" );      
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
      if (pirState == LOW) {
        digitalWrite(PIRoutPin, HIGH); //set 5V for PIR
        fona.sendSMS(mobileNumber, "PIR activated!" );
        pirState = HIGH;
      } else {
        digitalWrite(PIRoutPin, LOW);
        fona.sendSMS(mobileNumber, "PIR deactivated!" );
        pirState = LOW;
      }

      caseCounter = 0;
      timer = 0;
    break;
    } //End switch case
 } //End while timer
   

  //Enter low power Mode
  timer2 = 0;
  sleepInd = true;
  fona.enableGPS(false); //switch GPS off
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
  
} //End Loop



//---------------------------
//++++++++ FUNCTIONS ++++++++
//---------------------------

// read sent SMS and set caseCounter according to message to exectue desired action
void getSentSMS()
{
    char* bufPtr = fonaNotificationBuffer;    //handy buffer pointer
  
  if (fona.available())      //any data available from the FONA?
  {
    int slot = 0;            //this will be the slot number of the SMS
    int charCount = 0;
    //Read the notification into fonaInBuffer
    do  {
      *bufPtr = fona.read();
      delay(1);
    } while ((*bufPtr++ != '\n') && (fona.available()) && (++charCount < (sizeof(fonaNotificationBuffer)-1)));
    
    //Add a terminal NULL to the notification string
    *bufPtr = 0;

    //Scan the notification string for an SMS received notification.
    //  If it's an SMS message, we'll get the slot number in 'slot'
    if (1 == sscanf(fonaNotificationBuffer, "+CMTI: " FONA_PREF_SMS_STORAGE ",%d", &slot)) {
      
      char callerIDbuffer[32];  //we'll store the SMS sender number in here
      
      // Retrieve SMS sender address/phone number.
      fona.getSMSSender(slot, callerIDbuffer, 32);

        // Retrieve SMS value.
        uint16_t smslen;
        fona.readSMS(slot, smsBuffer, 25, &smslen);

      //Send back an automatic response, compare number of sender to mine
      if (memcmp(callerIDbuffer, mobileNumber, sizeof(mobileNumber)) == 0)
      {
        /*
         * caseCounter = 1 is reserved for PIR Alarm
         */
        if (memcmp(smsBuffer, option_1, sizeof(option_1)) == 0) {
          caseCounter = 2; //activate option 2
        }
        else if (memcmp(smsBuffer, option_2, sizeof(option_2)) == 0) {
          caseCounter = 3; //activate option 3
        }
        else if (memcmp(smsBuffer, option_3, sizeof(option_3)) == 0) {
          caseCounter = 4; //activate option 4
        }
        else if (memcmp(smsBuffer, option_4, sizeof(option_4)) == 0) {
          caseCounter = 5; //activate option 5
        }
        else if (memcmp(smsBuffer, option_5, sizeof(option_5)) == 0) {
          caseCounter = 6; //activate option 6
        }
        else if (memcmp(smsBuffer, option_6, sizeof(option_6)) == 0) {
          caseCounter = 7; //activate option 7
        }
        else {
          fona.sendSMS(mobileNumber, "Wrong Command!"); //send back SMS to inform about wrong command
        }

        timer = 0; //Reset timer after sms was recieved

        //fona.sendSMS(mobileNumber, smsBuffer); // Send back SMS
       
      } else {
        fona.sendSMS(mobileNumber, "Fremde Nummer schreibt mich an!"); //an unknown number has sent SMS to the tracker

       }
       
      
      // delete the original msg after it is processed
      //   otherwise, we will fill up all the slots
      //   and then we won't be able to receive SMS anymore
      //   delet slots as long as there are more then 1
      while (slot > 0) {
        if (fona.deleteSMS(slot)) {
          //Serial.println(F("OK!"));
        } else {
          //Serial.print(F("Couldn't delete SMS in slot ")); Serial.println(slot);
          fona.print(F("AT+CMGD=?\r\n"));
        }
        slot = slot - 1;
      }
    }
  }
 
  
}

void getGPS()
{
  gps_success = 0;
  int count2 = 0;
  
  delay(2000);

  while (!gps_success) { //Test if 3D fix possible. if not, exit while
    delay(2000);
    timer = 0; //Reset timer after each try
    // if you ask for an altitude reading, getGPS will return false if there isn't a 3D fix
    gps_success = fona.getGPS(&latitude, &longitude, &speed_kph, &heading, &altitude);
    count2++;
    
    if (gps_success) {

      char message2[128]; //prio 264
      char LAT[20];
      char LONG[20];  
      dtostrf(latitude, 0, 6,LAT);
      dtostrf(longitude, 0, 6,LONG);                  
      char *middle = "&ll=";
      char *endit = "&z=20";
  
      count2 = 0;
   
      sprintf(message2, "https://maps.google.com/?q=""%s,%s""%s""%s,%s%s",LAT,LONG,middle,LAT,LONG,endit);
      fona.sendSMS(mobileNumber, message2 );

  } 
  if (count2 == 15) {
    fona.sendSMS(mobileNumber, "No 3D Fix for 30s!");
    break;
  }
  
 }

}

void enableGPS()
{
      gps_success = 0;
      fona.enableGPS(true);
      delay(10000);
      
      // if you ask for an altitude reading, getGPS will return false if there isn't a 3D fix
       count1 = 0;
       
      while (!gps_success) { //Test if 3D fix possible. if not, exit while
        delay(2000);
        gps_success = fona.getGPS(&latitude, &longitude, &speed_kph, &heading, &altitude);
        count1++;
        timer = 0;
        
        if (gps_success) {          
         //Send SMS to my number
         fona.sendSMS(mobileNumber, "3D fix complete!" );

        }
        
        if (count1 == 15 && ! gps_success) {          
          //Send SMS to my number
          fona.sendSMS(mobileNumber, "3D fix not possible, try again!" );
          break;
        } 
      }

      gps_success = false;
      count1 = 0;
  
}

void simPowerOn()
{
  digitalWrite(gsmPower,LOW);
  delay(1000);
  digitalWrite(gsmPower,HIGH); //Turn GSM modul on 
  delay(1500);
  digitalWrite(gsmPower,LOW);
}

void simPowerOff()
{
  fona.enableGPS(false); //switch off GPS
  delay(2000);
  //fonaSerial->print("AT+CPOWD=1\n"); //Power Down GSM completely
  
   //alternativ to AT+CPOWD: 
   digitalWrite(gsmPower,LOW);
   delay(1000);
   digitalWrite(gsmPower,HIGH); //Turn GSM modul on when interrupt is triggered
   delay(1500);
   digitalWrite(gsmPower,LOW);
   
   delay(2000);
}

void simStart()
{
    // make it slow so its easy to read!
  fonaSerial->begin(4800);
  if (! fona.begin(*fonaSerial)) {
    //Couldn't find fona
    while(1);
  }

  //Enable GPS
  //Serial.println(F("Enabling GPS..."));
  //fona.enableGPS(true);

  // Print SIM card IMEI number.
  char imei[16] = {0}; // MUST use a 16 character buffer for IMEI!
  uint8_t imeiLen = fona.getIMEI(imei);
  if (imeiLen > 0) {
   // Serial.print("SIM card IMEI: "); Serial.println(imei);
  }

  fonaSerial->print("AT+CNMI=2,1\r\n");  //set up the FONA to send a +CMTI notification when an SMS is received
  
  //Send SMS to my number
  fona.sendSMS(mobileNumber, "Sim is active now!" );
 
}

// Interrupt Resets
void RItimerReset()
{
  timer = 0; //Reset timer on IR Pin interrupt for sleep mode
}

void PIRtimerReset() 
{
  timer = 0;       //Reset timer on PIR interrupt for sleep mode
  pirMotion = true; //activate ALARM
}

