#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
//#include <ESP8266HTTPClient.h>
#endif
#include <PubSubClient.h>
#include <DebugMacros.h>

#include <Arduino_JSON.h>
#include "config.h"
#include <SPI.h>
#include <MFRC522.h>
MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class
MFRC522::MIFARE_Key key;
// Init array that will store new NUID
byte nuidPICC[4];


extern const char* host;


bool door_locked = true;
bool door_closed = false;
unsigned long door_open = 0;
unsigned long last_open = 0;

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0;
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 10000;


char cmd = CMD_NOOP;

String msgToSend = "";
String nullstring = "";
String logTrigger = "";
String output4State = "off";
int rfidLastUpdate = RFIDUPDATETIME;
bool rfidinit = false;

// Use @myidbot to find out the chat ID of an individual or a group
// Also note that you need to click "start" on a bot before it can
// message you
#define CHAT_ID CHAT_ID_2



WiFiClient espClient;
PubSubClient mqttclient(espClient);

// Checks for new messages every 1 second.
int botRequestDelay = 1000;
unsigned long lastTimeBotRan;

const int ledPin = 2;
bool ledState = LOW;

uint8_t source = CMD_SOURCE_SERIAL;


void setup() {
  Serial.begin(115200);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, LOW);

  pinMode(DOOR_C, OUTPUT);
  pinMode(DOOR_NC, INPUT_PULLUP);


  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  // Print ESP32 Local IP Address
  Serial.println(WiFi.localIP());

  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522
  Serial.println();
  Serial.print(F("Reader :"));
  rfid.PCD_DumpVersionToSerial();
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }
  Serial.println();
  Serial.println(F("This code scan the MIFARE Classic NUID."));
  Serial.print(F("Using the following key:"));
  printHex(key.keyByte, MFRC522::MF_KEY_SIZE);
  Serial.println();
  initMQTT();
}


void mqtt_connect() {
  if (!mqttclient.connected()) {
    while (!mqttclient.connected()) {
      String client_id = "esp8266-client-";
      client_id += String(WiFi.macAddress());
      Serial.println("Connecting to public emqx mqtt broker.....");
      if (mqttclient.connect((const char*) client_id.c_str(), mqtt_username, mqtt_password)) {
        Serial.println("Public emqx mqtt broker connected");
      } else {
        Serial.print("failed with state ");
        Serial.print(mqttclient.state());
        delay(2000);
      }
    }
    // publish and subscribe
    mqttclient.publish(topic_talk, "hello emqx");
    mqttclient.subscribe(topic_listen);
  }
}

void initMQTT() {
  //connecting to a mqtt broker
  mqttclient.setServer(mqtt_broker, mqtt_port);
  mqttclient.setCallback(callback);
  mqtt_connect();
}
void loop() {
  get_serial();
  get_button();
  getDoorStatus();
  process_rfid();
  mqtt_connect();
  mqttclient.loop();
  //---COMMANDS--------
  if (cmd != CMD_NOOP) {
    process_cmd();
  }

  closeDoorOnTimeout();

  if (!(logTrigger == "")) {
    reportActivity();
  }
  tagID = "";
  delay(20);
}

void callback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
  Serial.print("Message:");
  String message;
  for (int i = 0; i < length; i++) {
    message = message + (char) payload[i];  // convert *byte to string
  }
  Serial.println(message);
  if (message == "open") {
    cmd = CMD_OPEN;
    source = CMD_SOURCE_MQTT;
  } else if (message == "close") {
    cmd = CMD_CLOSE;
    source = CMD_SOURCE_MQTT;
  } else
  {
    Serial.println(F("unknown command"));//
    Serial.println(message);
  }
  Serial.println("-----------------------");
}

void reportActivity() {
  msgToSend =  "Log: ";
  if (logTrigger == "button") {
    msgToSend += " button pressed";
  } else if (logTrigger == "rfid") {
    msgToSend += "Access granted to rfid%: " + tagID + " (" + tagName + ")";
  } else if (logTrigger == "alert") {
    msgToSend += "Access denied to rfid%: " + tagID + " (" + tagName + ")";
  } else if (logTrigger == "heartbit") {
    msgToSend += "heartbit";
  }
  Serial.println("POST data to MQTT:");
  Serial.println(msgToSend);


  mqttclient.publish(topic_talk, msgToSend.c_str());
  logTrigger = "";
  msgToSend = "";
}



void closeDoorOnTimeout() {
  if (door_locked == false) {
    if (millis() - last_open > TIMEON) {
      digitalWrite(PIN_RELAY, LOW);
      door_locked = true;
      Serial.println("Closed");
    }
  }
}

void get_serial() {
  if (Serial.available()) {
    cmd = Serial.read();
    source = CMD_SOURCE_SERIAL;
  }
   
  if ((cmd == '\r') || (cmd == '\n')) {
    cmd = CMD_NOOP;
  }
}

void get_button(){
  static unsigned long last_check = 0;
  if (millis() - last_check > 2000){
   if (digitalRead(PIN_BUTTON) == LOW) {
      cmd = CMD_OPEN;
      source = CMD_SOURCE_BUTTON;
      logTrigger = "button";
      last_check = millis();
    }else { 
      delay(10);
    }
  }
}

void getDoorStatus() {
  digitalWrite(DOOR_C, HIGH);
  delay(5);
  if (door_locked == true) {
    if (digitalRead(DOOR_NC) != LOW) {
      door_closed = false;
    }
  }
  digitalWrite(DOOR_C, LOW);
}

void process_rfid() {
  if (getID()) {
    if (!tagID.equals(String(""))) query_access(tagID);
  }
}

void process_cmd() {
  Serial.println(&cmd);
  switch (tolower(cmd)) {
    case CMD_STATUS:
      if (door_locked == false) {
        Serial.println("Open");
      } else {
        Serial.println("Closed");
      }
      break;
    case CMD_OPEN:
      digitalWrite(PIN_RELAY, HIGH);
      last_open = millis();
      door_locked = false;
      Serial.println("Opened");
      break;
    case CMD_CLOSE:
      digitalWrite(PIN_RELAY, LOW);
      Serial.println("Closed");
      door_locked = true;
      break;
    case CMD_HELP:
      Serial.println("?\tHELP");
      Serial.println("x\tCLOSE");
      Serial.println("v\tOPEN");
      Serial.println("s\tSTATUS");
      break;
  }
  cmd = CMD_NOOP;
}


void query_access(String tagID) {
  String authorize = "authorize:";
  authorize += tagID;
  Serial.println(authorize);
  mqttclient.publish(topic_talk, authorize.c_str());
}

//Read new tag if available
boolean getID() {
  // Getting ready for Reading PICCs
  if (!rfid.PICC_IsNewCardPresent()) {  //If a new PICC placed to RFID reader continue
    return false;
  }
  if (!rfid.PICC_ReadCardSerial()) {  //Since a PICC placed get Serial and continue
    return false;
  }
  tagID = "";
  for (uint8_t i = 0; i < 4; i++) {  // The MIFARE PICCs that we use have 4 byte UID
    //readCard[i] = rfid.uid.uidByte[i];
    tagID.concat(String(rfid.uid.uidByte[i], HEX));  // Adds the 4 bytes in a single String variable
  }
  //byte readbackblock[18];
  tagID.toUpperCase();
  Serial.println(tagID);
  rfid.PICC_HaltA();  // Stop reading
  return true;
}


bool authorize(String _tagID) {
  if (door_locked == true) {
    source = CMD_SOURCE_MQTT;
    cmd = CMD_OPEN;
    Serial.println(" Access Granted!  ");
    logTrigger = "mqtt";
    // You can write any code here like opening doors, switching on a relay, lighting up an LED, or anything else you can think of.
  } else {
    Serial.println(" Access Denied!  ");
    logTrigger = "alert";
  }
}




/**
    Helper routine to dump a byte array as hex values to Serial.
*/
void printHex(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}
/**
    Helper routine to dump a byte array as dec values to Serial.
*/
void printDec(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], DEC);
  }
}
