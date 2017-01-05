/****************************************************************************************************************************\
   Arduino project "Nano Easy" © Copyright www.letscontrolit.com

   This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
   You received a copy of the GNU General Public License along with this program in file 'License.txt'.

   IDE download    : https://www.arduino.cc/en/Main/Software

   Source Code     : https://github.com/ESP8266nu/ESPEasy
   Support         : http://www.letscontrolit.com
   Discussion      : http://www.letscontrolit.com/forum/

   Additional information about licensing can be found at : http://www.gnu.org/licenses
  \*************************************************************************************************************************/

// This file incorporates work covered by the following copyright and permission notice:

/****************************************************************************************************************************\
  Arduino project "Nodo" © Copyright 2010..2015 Paul Tonkes

  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
  You received a copy of the GNU General Public License along with this program in file 'License.txt'.

  Voor toelichting op de licentievoorwaarden zie    : http://www.gnu.org/licenses
  Uitgebreide documentatie is te vinden op          : http://www.nodo-domotica.nl
  Compiler voor deze programmacode te downloaden op : http://arduino.cc
  \*************************************************************************************************************************/

// A very limited Easy project as the Nano has only 30k flash for program code and only 2k of RAM.
// We skipped 99% of the Arduino Easy code to get it compiled.
// Due to very limited code size, these things should be avoided or they are just plain impossible...:
//   - DHCP
//   - using floats
//   - using web style sheet elements
//   - using dropdown lists

// That actually leaves us with not much more than a Domoticz HTTP sensor/actuator with a single io pin.

#define DEFAULT_IP          {192,168,0,254}     // default IP if no IP setting
#define DEFAULT_NAME        "new"               // Enter your device friendly name
#define DEFAULT_SERVER      "192.168.0.8"       // Enter your Domoticz Server IP address
#define DEFAULT_PORT        8080                // Enter your Domoticz Server port value
#define DEFAULT_DELAY       60                  // Enter your Send delay in seconds
#define DEFAULT_UNIT        0                   // Enter your default unit number, must be UNIQUE for each unit!
#define UDP_PORT            65500

#define FEATURE_CONTROLLER_DOMOTICZ true
#define FEATURE_CONTROLLER_MQTT     false // does not work with webconfig, sketch to big...
#define FEATURE_UDP_SEND            true
#define FEATURE_UDP_RECV            false
#define FEATURE_HTTP_AUTH           false
#define FEATURE_WEB_CONFIG_NETWORK  true
#define FEATURE_WEB_CONFIG_MAIN     true
#define FEATURE_WEB_CONTROL         true
#define FEATURE_SERIAL_DEBUG        true


// do not change anything below this line !

#define NANO_PROJECT_PID       2016120701L
#define VERSION                          1
#define BUILD                          148

#define NODE_TYPE_ID_NANO_EASY_STD         81
#define NODE_TYPE_ID                       NODE_TYPE_ID_NANO_EASY_STD

#include <EEPROM.h>
#include <UIPEthernet.h>
#include <UIPServer.h>
#include <UIPClient.h>

#if FEATURE_CONTROLLER_MQTT
#include <PubSubClient.h>
EthernetClient mqtt;
PubSubClient MQTTclient(mqtt);
#endif

EthernetServer WebServer = EthernetServer(80);
#if FEATURE_UDP_SEND or FEATURE_UDP_RECV
EthernetUDP portUDP;
#endif

void(*Reboot)(void) = 0;

struct SettingsStruct
{
  unsigned long PID;
  int           Version;
  byte          Unit;
  int16_t       Build;
  byte          IP[4];
  byte          Gateway[4];
  byte          Subnet[4];
  byte          DNS[4];
  byte          Controller_IP[4];
  unsigned int  ControllerPort;
  unsigned long Delay;
  char          Name[26];
  int16_t       IDX;
  byte          Pin;
  byte          Mode;
#if FEATURE_HTTP_AUTH
  char          ControllerUser[26];
  char          ControllerPassword[64];
#endif
#if FEATURE_CONTROLLER_MQTT
  char          MQTTsubscribe[80];
  boolean       MQTTRetainFlag;
#endif
} Settings;

unsigned long timer;
unsigned long timerwd;
unsigned long wdcounter;

void setup()
{
#if FEATURE_SERIAL_DEBUG
  Serial.begin(115200);
#endif
  LoadSettings();
  if (Settings.Version == VERSION && Settings.PID == NANO_PROJECT_PID)
  {
#if FEATURE_SERIAL_DEBUG
    Serial.println("OK");
#endif
  }
  else
  {
#if FEATURE_SERIAL_DEBUG
    Serial.print("R!");
#endif
    delay(1000);
    ResetFactory();
  }

  if (Settings.Build != BUILD)
  {
    Settings.Build = BUILD;
    SaveSettings();
  }

  uint8_t mac[6] = {0x00, 0x01, 0x02, 0x03, 0x04, Settings.Unit};
  IPAddress myIP;
  if (Settings.IP[0] != 0)
    myIP = Settings.IP;
  else
    myIP = DEFAULT_IP;

  Ethernet.begin(mac, myIP);
  // todo: begin(const uint8_t* mac, IPAddress ip, IPAddress dns, IPAddress gateway, IPAddress subnet);

  WebServer.begin();
#if FEATURE_UDP_RECV
  portUDP.begin(UDP_PORT);
#endif

#if FEATURE_CONTROLLER_MQTT
  MQTTConnect();
#endif

  timer = millis() + Settings.Delay * 1000;
  timerwd = millis() + 30000;
}

void loop()
{
  if (millis() > timerwd)
  {
    timerwd = millis() + 30000;
    wdcounter++;
#if FEATURE_UDP_SEND
    UDP();
#endif
  }

  if (Settings.Delay != 0)
  {
    if (millis() > timer)
    {
#if FEATURE_SERIAL_DEBUG
      Serial.println("S");
#endif
      timer = millis() + Settings.Delay * 1000;
      int value = 0;
      switch (Settings.Mode)
      {
        case 1:
          value = analogRead(Settings.Pin);
          break;
        case 2:
          value = digitalRead(Settings.Pin);
          break;
      }
#if FEATURE_CONTROLLER_DOMOTICZ
      Domoticz_sendData(Settings.IDX, value);
#endif
#if FEATURE_CONTROLLER_MQTT
      String svalue = String(value);
      MQTTclient.publish(Settings.Name, svalue.c_str());
#endif
    }
  }
  WebServerHandleClient();
#if FEATURE_UDP_RECV
  UDPCheck();
#endif
#if FEATURE_CONTROLLER_MQTT
  MQTTclient.loop();
#endif
}

