/**
 * The MySensors Arduino library handles the wireless radio link and protocol
 * between your home built sensors/actuators and HA controller of choice.
 * The sensors forms a self healing radio network with optional repeaters. Each
 * repeater and gateway builds a routing tables in EEPROM which keeps track of the
 * network topology allowing messages to be routed to nodes.
 *
 * Created by Henrik Ekblad <henrik.ekblad@mysensors.org>
 * Copyright (C) 2013-2015 Sensnology AB
 * Full contributor list: https://github.com/mysensors/Arduino/graphs/contributors
 *
 * Documentation: http://www.mysensors.org
 * Support Forum: http://forum.mysensors.org
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 *******************************
 * Originally based on work by GizMoCuz (Domoticz). 
 * 
 * Converted to platform-io, and cpp. 
 * Heavily refactored for non-blocking use, with statemachine for touch / dimming interaction
 * 
 * Uses a MPR121 for touch interface, and sets an analog out (PWM) that controls a LED strip
 * current implementation does not distinguish on the different touch buttons
 */
#include <Arduino.h>
#include "ProjectDefs.h"
#include "StateMachine.h"
// Enable debug prints
#define MY_DEBUG

// Enable and select radio type attached
#define MY_RADIO_RF24
//#define MY_RADIO_RFM69
#define MY_REPEATER_FEATURE

#include <MySensors.h>
#include <Adafruit_MPR121.h>
#include <Wire.h>

#define CHILD_ID_LIGHT 1

const uint8_t EPROM_LIGHT_1 = 0;
const uint8_t EPROM_LIGHT_2 = 1;

#define SN "Dimable Light-touch"
#define SV "2.0"

Adafruit_MPR121 cap = Adafruit_MPR121();

MyMessage dimmerMsg(CHILD_ID_LIGHT, V_DIMMER);
void sendCurrentState2Controller(int16_t desiredLevel, bool onOff);

int16_t readFromEeprom() {
  uint8_t byte1 = loadState(EPROM_LIGHT_1);
  uint8_t byte2 = loadState(EPROM_LIGHT_2);
  int16_t storedValue = byte1 * 0xff + byte2;
  Serial.print("reading from eeprom: ");
  Serial.println(storedValue); 
  return storedValue;
}

void saveToEeprom(int16_t value) {
  int16_t oldValue = readFromEeprom();
  // Only save, if the difference is larger than 5
  // this is to save a bit on eeprom writes.
  if (oldValue != value) {
    Serial.print("saving to eeprom: ");
    Serial.println(value);
    uint8_t byte1 = value / 0xff;
    uint8_t byte2 = value % 0xff;
    saveState(EPROM_LIGHT_1, byte1);
    saveState(EPROM_LIGHT_2, byte2);
  }
}

void setup()
{
  Serial.begin(115200);
  pinMode(IRQ_PIN, INPUT);
  digitalWrite(IRQ_PIN, HIGH);

  pinMode(LED_PIN, OUTPUT);
  analogWrite(LED_PIN, 0);

	Serial.println("Node ready to receive messages...");

  if (!cap.begin()) {
    Serial.println("MPR121 not found!");
    while(1);  
  }
  Serial.println("MPR121 found, continuing");
  int16_t storedValue = readFromEeprom();
  InitSM(sendCurrentState2Controller, storedValue);
}

void presentation()
{
	// Send the Sketch Version Information to the Gateway
	sendSketchInfo(SN, SV);

	present(CHILD_ID_LIGHT, S_DIMMER );
}

/** 
 * MySensor receive method 
 */
void receive(const MyMessage &message)
{
  switch(message.type) {
  
    case V_LIGHT: {
      //When receiving a V_LIGHT command we switch the light between OFF and the last received dimmer value
      //This means if you previously set the lights dimmer value to 50%, and turn the light ON
      //it will do so at 50%
      Serial.println("V_LIGHT command received...");
  
      int lstate= constrain(atoi( message.data ), 0, 1);
      controllerOnOff(lstate);
      break;
    }
    case V_PERCENTAGE: {
      Serial.println("V_DIMMER command received...");
      int16_t dimvalue= atoi( message.data );
      controllerValue(dimvalue);
      break;
    }
    default:
      Serial.println("Invalid command received...");
      return;
  }
}

/***
 * Send desired level to mysensors controller, if changed from last time it was sent
 */
void sendCurrentState2Controller(int16_t transmitLevel, bool onOff)
{
  static int16_t lastTransmittedLevel;
  int16_t level = transmitLevel;
  if (!onOff) {
    level = 0;
  }
  
  if (lastTransmittedLevel != level) {
    send(dimmerMsg.set((int16_t)level));
    saveToEeprom(level);
    lastTransmittedLevel = level;
  }
}

void loop() {
  static uint16_t lastTouched;
  uint16_t touched = cap.touched();
  UpdateSM();
  if (lastTouched != touched) {
    touch(touched > 0);
    lastTouched = touched;
  }
}