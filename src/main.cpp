#include <Arduino.h>
#include <SPI.h>

#include <Wire.h>
#include <Adafruit_AMG88xx.h>

#include "uRTCLib.h"

#include <WiFiNINA.h>
#include <WiFiUdp.h>

#define PIN_RELAY_PUMP 1

int status = WL_IDLE_STATUS;
#include "arduino_secrets.h"
///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID; // your network SSID (name)
char pass[] = SECRET_PASS; // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;          // your network key Index number (needed only for WEP)

unsigned int localPort = 2390; // local port to listen on

char packetBuffer[256]; // buffer to hold incoming packet

Adafruit_AMG88xx amg;

uRTCLib rtc(0x68);

float pixels[AMG88xx_PIXEL_ARRAY_SIZE];

void setup()
{
  pinMode(PIN_RELAY_PUMP, OUTPUT);

  Serial.begin(9600);
  Serial.println(F("AMG88xx pixels"));

  URTCLIB_WIRE.begin();


  //rtc.set(0, 04, 15, 5, 30, 3, 23);

  if (rtc.enableBattery())
  {
    Serial.println("Battery activated correctly.");
  }
  else
  {
    Serial.println("ERROR activating battery.");
  }

  bool status;

  // default settings
  status = amg.begin();
  if (!status)
  {
    Serial.println("Could not find a valid AMG88xx sensor, check wiring!");
    while (1)
      ;
  }

  Serial.println("-- Pixels Test --");

  Serial.println();

  delay(100); // let sensor boot up
}

void loop()
{
  // read all the pixels
  amg.readPixels(pixels);

  for (int i = 1; i <= AMG88xx_PIXEL_ARRAY_SIZE; i++)
  {
    Serial.print(pixels[i - 1]);
    Serial.print(", ");
    if (i % 8 == 0)
      Serial.println();
  }
  Serial.println();

  delay(1000);

  rtc.refresh();

  Serial.print("RTC DateTime: 20");
  Serial.print(rtc.year());
  Serial.print('-');
  Serial.print(rtc.month());
  Serial.print('-');
  Serial.print(rtc.day());

  Serial.print(' ');

  Serial.print(rtc.hour());
  Serial.print(':');
  Serial.print(rtc.minute());
  Serial.print(':');
  Serial.print(rtc.second());

  Serial.print(" DOW: ");
  Serial.print(rtc.dayOfWeek());

  Serial.print(" - Temp: ");
  Serial.print(amg.readThermistor());

  Serial.println();

  digitalWrite(PIN_RELAY_PUMP, HIGH);
  // delay a second
  delay(2000);
  digitalWrite(PIN_RELAY_PUMP, LOW);
}