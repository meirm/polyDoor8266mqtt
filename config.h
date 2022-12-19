#ifndef _DOORLOCK
#define _DOORLOCK
#include "secure.h"
// Definitions
#define PIN_BUTTON 4
#define PIN_RELAY 2
#define TIMEON 5 * 1000
#define SENDFREQ 20 * 1000  //unused
//rfid

#define DEBUG false
#define DOOR_C 5
#define DOOR_NC 0


#define SS_PIN D8
#define RST_PIN D0


/*
Vcc <-> 3V3 (or Vin(5V) depending on the module version)
RST (Reset) <-> D0
GND (Masse) <-> GND
MISO (Master Input Slave Output) <-> 19
MOSI (Master Output Slave Input) <-> 23
SCK (Serial Clock) <-> 18
SS/SDA (Slave select) <-> 5
*/
#define ON 1
#define OFF 0
// PROTOCOLS
#define WIFION ON
#define SSID "polyhedra"
#define PASSWD "polyhedra3d"

// COMMANDS
#define CMD_HELP '?'
#define CMD_OPEN 'v'
#define CMD_CLOSE 'x'
#define CMD_STATUS 's'
#define CMD_NOOP '\0'

#define CMD_SOURCE_BT 0
#define CMD_SOURCE_SERIAL 1
#define CMD_SOURCE_BUTTON 2
#define CMD_SOURCE_RFID 3
#define CMD_SOURCE_MQTT 4
#define RFIDUPDATETIME 10 * 60 * 1000


//GLOBAL VARS
byte readCard[4];

const unsigned long BOT_MTBS = 1000;
String tagID = "";
String tagName = "Nishta";
boolean getID();
void query_access(String tagID);

#endif
