#include <Arduino.h>
#include <SPI.h>

#include <Wire.h>
#include <Adafruit_AMG88xx.h>

#include "uRTCLib.h"

#include <WiFiNINA.h>
#include <WiFiUdp.h>

#include <stdio.h>

#define PIN_RELAY_PUMP 1

int status = WL_IDLE_STATUS;
#include "arduino_secrets.h"
///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID; // your network SSID (name)
char pass[] = SECRET_PASS; // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;          // your network key Index number (needed only for WEP)

unsigned int localPort = 2390; // local port to listen on

char packetBuffer[256];                          // buffer to hold incoming packet
char replyBuffer[AMG88xx_PIXEL_ARRAY_SIZE]; // a string to send back

WiFiUDP Udp;

Adafruit_AMG88xx amg;

uRTCLib rtc(0x68);

float pixels[AMG88xx_PIXEL_ARRAY_SIZE];

void printWifiStatus();
void handleUdp();
void handleRtc();

void setup()
{
  pinMode(PIN_RELAY_PUMP, OUTPUT);

  Serial.begin(9600);
  Serial.println(F("AMG88xx pixels"));

  URTCLIB_WIRE.begin();

  if (WiFi.status() == WL_NO_MODULE)
  {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true)
      ;
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION)
  {
    Serial.println("Please upgrade the firmware");
  }

  // attempt to connect to WiFi network:
  while (status != WL_CONNECTED)
  {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(10000);
  }
  Serial.println("Connected to WiFi");
  printWifiStatus();

  Serial.println("\nStarting connection to server...");
  // if you get a connection, report back via serial:
  Udp.begin(localPort);

  // rtc.set(0, 04, 15, 5, 30, 3, 23);

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
    // IMPORTANT: FIX
    replyBuffer[i - 1] = pixels[i - 1];
  }

    handleUdp();

    delay(1000);

    handleRtc();

    // digitalWrite(PIN_RELAY_PUMP, HIGH);
    //  delay a second
    // delay(1000);
    // digitalWrite(PIN_RELAY_PUMP, LOW);
  }

void printWifiStatus()
{
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

void handleUdp()
{
  int packetSize = Udp.parsePacket();
  if (packetSize)
  {
    Serial.print("Received packet of size ");
    Serial.println(packetSize);
    Serial.print("From ");
    IPAddress remoteIp = Udp.remoteIP();
    Serial.print(remoteIp);
    Serial.print(", port ");
    Serial.println(Udp.remotePort());

    // read the packet into packetBufffer
    int len = Udp.read(packetBuffer, 255);
    if (len > 0)
    {
      packetBuffer[len] = 0;
    }
    Serial.println("Contents:");
    Serial.println(packetBuffer);

    // send a reply, to the IP address and port that sent us the packet we received
    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
    Udp.write(replyBuffer);
    Udp.endPacket();
  }
}

void handleRtc()
{
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
}