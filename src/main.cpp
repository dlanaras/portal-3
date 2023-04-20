#include <Arduino.h>
#include <SPI.h>

#include <Wire.h>
#include <Adafruit_AMG88xx.h>

#include "uRTCLib.h"

#include <WiFiNINA.h>
#include <WiFiUdp.h>
#include <stdio.h>

#include <string>

#include <Servo.h>

#define PIN_RELAY_PUMP 1
#define PIN_ROTATO 7
#define MAX_ROTATO 180
#define MIN_ROTATO 0

int status = WL_IDLE_STATUS;
#include "arduino_secrets.h"
///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID; // your network SSID (name)
char pass[] = SECRET_PASS; // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;          // your network key Index number (needed only for WEP)

unsigned int firstPort = 8081; // local port to listen on
unsigned int secondPort = 8082;

char packetBuffer[256]; // buffer to hold incoming packet

char packetSizeString[255];
const char format[] = "20%d-%d-%d %d:%d:%d DOW: %d";
char datetime[35] = "";
char thermalData[317] = "";
int sentryMode = 1;
int rotation = 15;
int direction = 0;

Servo rotatoServo;

WiFiUDP Udp;

WiFiUDP PumpUdp;

Adafruit_AMG88xx amg;

uRTCLib rtc(0x68);

#include <SPI.h>
// #include <SD.h>
#include "SdFat.h"
SdFat SD;

#define SD_CS_PIN SS
File logFile;

float pixels[AMG88xx_PIXEL_ARRAY_SIZE];

void printWifiStatus();
void UdpSendRtc();
void UdpSendThermal();
void UdpSendContent(const char content[]);
void logInformation(const char *content);
const char *readLogFile();

void setup()
{
  pinMode(PIN_RELAY_PUMP, OUTPUT);

  if (!SD.begin(SD_CS_PIN))
  {
    Serial.println("initialization failed!");
    return;
  }
  logFile = SD.open("test.txt", FILE_WRITE);

  logInformation("initialization done.");

  Serial.begin(9600);
  logInformation("AMG88xx pixels");

  rotatoServo.attach(PIN_ROTATO);

  URTCLIB_WIRE.begin();

  if (WiFi.status() == WL_NO_MODULE)
  {
    logInformation("Communication with WiFi module failed!");
    // don't continue
    while (true)
      ;
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION)
  {
    logInformation("Please upgrade the firmware");
  }

  // attempt to connect to WiFi network:
  while (status != WL_CONNECTED)
  {
    logInformation("Attempting to connect to SSID: ");
    logInformation(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(10000);
  }
  logInformation("Connected to WiFi");
  printWifiStatus();

  logInformation("\nStarting connection to server...");
  // if you get a connection, report back via serial:
  Udp.begin(firstPort);
  PumpUdp.begin(secondPort);

  // rtc.set(0, 10, 13, 5, 6, 4, 23);

  if (rtc.enableBattery())
  {
    logInformation("Battery activated correctly.");
  }
  else
  {
    logInformation("ERROR activating battery.");
  }

  Serial.print("Initializing SD card...");

  bool status;

  // default settings
  status = amg.begin();
  if (!status)
  {
    logInformation("Could not find a valid AMG88xx sensor, check wiring!");
    while (1)
      ;
  }

  logInformation("-- Pixels Test --");

  delay(100); // let sensor boot up
}

void loop()
{
  rtc.refresh();
  rotatoServo.write(rotation);

  amg.readPixels(pixels);

  UdpSendContent("BEGINT");
  UdpSendThermal();
  UdpSendContent("ENDT");
  UdpSendContent("BEGINL");
  UdpSendContent(readLogFile());
  UdpSendContent("ENDL");
  
  int packetSize = PumpUdp.parsePacket();
  if (packetSize)
  {
    logInformation("Received packet of size ");

    sprintf(packetSizeString, "%d", packetSize);
    logInformation(packetSizeString);

    // read the packet into packetBufffer
    int len = PumpUdp.read(packetBuffer, 255);
    if (len > 0)
    {
      packetBuffer[len] = 0;
    }
    logInformation("Contents:");
    logInformation(packetBuffer);

    if (strcmp(packetBuffer, "SAP") == 0) // spray and pray
    {
      digitalWrite(PIN_RELAY_PUMP, HIGH);
      sentryMode = 0;
    }
    else if (strcmp(packetBuffer, "SAD") == 0) // stop and drop
    {
      digitalWrite(PIN_RELAY_PUMP, LOW);
      sentryMode = 0;
    }
    else if (strcmp(packetBuffer, "RESUMÉ") == 0)
    {
      sentryMode = 1;
    }
  }

  packetSize = Udp.parsePacket();
  if (packetSize)
  {
    int pos;
    logInformation("Received packet of size ");
    sprintf(packetSizeString, "%d", packetSize);
    logInformation(packetSizeString);

    // read the packet into packetBufffer
    int len = Udp.read(packetBuffer, 255);
    if (len > 0)
    {
      packetBuffer[len] = 0;
    }
    logInformation("Contents:");
    logInformation(packetBuffer);

    sscanf(packetBuffer, "%d", &pos);

    if (pos >= MIN_ROTATO && pos <= MAX_ROTATO)
    {
      rotation = pos;
    }
    // send a reply, to the IP address and port that sent us the packet we received
  }

  if (sentryMode)
  {
    int weight[3] = {0, 0, 0};
    for (int i = 1; i <= AMG88xx_PIXEL_ARRAY_SIZE; i++)
    {
      int displacement = i % 8;
      if (pixels[i - 1] > 70)
      {

        if (displacement < 4) // to left
        {
          weight[0]++;
        }
        else if (displacement > 5) // to right
        {
          weight[2]++;
        }
        else // in middle rows | | | (| |) | | |
        {
          weight[1]++;
        }
      }
    }

    if (weight[0] > weight[1] && weight[0] > weight[2]) // go left
    {
      rotation -= 10;
    }
    else if (weight[1] > weight[0] && weight[1] > weight[2]) // spurt water
    {
      digitalWrite(PIN_RELAY_PUMP, HIGH);
      delay(5000);
      digitalWrite(PIN_RELAY_PUMP, LOW);
    }
    else if (weight[2] > weight[0] && weight[2] > weight[1]) // go right
    {
      rotation += 10;
    }
    else
    {
      if (rotation >= MAX_ROTATO || rotation <= MIN_ROTATO)
      {
        direction = !direction;
      }

      if (direction)
      {
        rotation += 10;
      }
      else
      {
        rotation -= 10;
      }
    }
  }
}

void printWifiStatus()
{
  // print the SSID of the network you're attached to:
  logInformation("SSID: ");
  logInformation(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  logInformation("IP Address: ");
  char ipString[20];

  String(ip).toCharArray(ipString, sizeof(ipString));

  logInformation(ipString);
}

void UdpSendThermal()
{
  for (int i = 0; i < AMG88xx_PIXEL_ARRAY_SIZE; i++)
  {
    sprintf(thermalData, "%f", pixels[i]);

    UdpSendContent(thermalData);
  }
}

void UdpSendContent(const char content[])
{
  Udp.beginPacket("10.42.0.82", 8081);
  Udp.write(content);
  Udp.endPacket();
}

void logInformation(const char *content)
{
  if (logFile)
  {
    sprintf(datetime, format, rtc.year(), rtc.month(), rtc.day(), rtc.hour(), rtc.minute(), rtc.second(), rtc.dayOfWeek());

    logFile.print(datetime);
    logFile.println(content);
    // close the file:
    logFile.close();
  }
  else
  {
    // if the file didn't open, print an error:
    Serial.println("error opening test.txt");
  }
}

const char *readLogFile()
{
  if (logFile)
  {
    std::string logs;
    // read from the file until there's nothing else in it:
    while (logFile.available())
    {
      logs += logFile.read();
    }

    return logs.c_str();
  }
  else
  {
    // if the file didn't open, print an error:
    Serial.println("error opening test.txt");
    return "Error opening test.txt";
  }
}