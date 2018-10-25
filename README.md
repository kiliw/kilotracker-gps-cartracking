# kilotracker-gps-cartracking

If you want to secure your car you can use this code in combination with an Arduino Uno or Nano and a Sim808 module to get the location of your vehicle.

So far the code is far from beeing perfect but it works. The goal is to improve it further so its easy to understand, use and customize to your own wishes.

# ToDo

- [ ] include low power mode of gsm

- [ ] get interrupts working

- [ ] how to integrate interrupt on PIR when Arduino is in sleep mode and has no output voltage, maybe it has output voltage

- [ ] PIR alarm triggers after 1 min, because pirMotion = true during interrupt

# Usage

## Required Libraries

- Adafruit_FONA
- LowPower
- SoftwareSerial

## Arduino Setup

- Arduino Uno/Nano
- Sim 808
- Sim card
- PIR Sensor (optional)

## Configuration

After you've assembled your Arduino setup, go and create a `number_config.h` and add the following content:

`char mobileNumber[] = "+4912345678";`

This should be your own mobile number. You will get the location send via SMS and a link to googlemaps from the tracker. The program will also send a notification to this number if a foreign number is texting to it.

## Commands

The desired command has to be sent via SMS to the number in the Sim808. At the moment the following commands can be used:

`GPSON`: Switch on the GPS module

`GPSOF`: Switch of the GPS module

`GPSLO`: Get the current GPS location of the tracker

## Example
