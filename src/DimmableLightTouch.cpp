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
 *
 * REVISION HISTORY
 * Version 1.0 - January 30, 2015 - Developed by GizMoCuz (Domoticz)
 *
 * DESCRIPTION
 * This sketch provides an example how to implement a Dimmable Light
 * It is pure virtual and it logs messages to the serial output
 * It can be used as a base sketch for actual hardware.
 * Stores the last light state and level in eeprom.
 *
 */
#include <Arduino.h>
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

#define EPROM_LIGHT_STATE 1
#define EPROM_DIMMER_LEVEL 2

#define LIGHT_OFF false
#define LIGHT_ON true

#define SN "Dimable Light-touch"
#define SV "1.1"

#define LED_PIN1 6

const int IRQ_PIN = 3;
const uint16_t DIMMER_TOUCH_MASK = 2047;

// Below is time given in milli seconds.
const long TOUCH_TIME_DIMMING = 500;
const long TOUCH_TIME_TOGGLE_POWER = 100;
const long TOUCH_TIME_BETWEEN_ACTIONS = 500;
const long FADE_DELAY = 10;  // Delay in ms for each percentage fade up/down (10ms = 1s full-range dim)

boolean currentState=LIGHT_OFF;
int16_t desiredLevel=50;

enum Direction {
  UP,
  DOWN
};

Direction dimmerDirection = UP;

Adafruit_MPR121 cap = Adafruit_MPR121();

MyMessage dimmerMsg(CHILD_ID_LIGHT, V_DIMMER);
void handleDimming();
void fadeToDesiredLevel();
void SendCurrentState2Controller();
void handlePowerToggle();

void setup()
{
  Serial.begin(115200);
  pinMode(IRQ_PIN, INPUT);
  digitalWrite(IRQ_PIN, HIGH);
  
  pinMode(LED_PIN1, OUTPUT);
  analogWrite(LED_PIN1, 0);
  //Retreive our last light state from the eprom
	int LightState=loadState(EPROM_LIGHT_STATE);
	if (LightState<=1) {
		currentState=LightState;
		int DimValue=loadState(EPROM_DIMMER_LEVEL);
		if ((DimValue>0)&&(DimValue<=100)) {
			//There should be no Dim value of 0, this would mean LIGHT_OFF
			desiredLevel=DimValue;
		}
	}
 
	Serial.println( "Node ready to receive messages..." );

  if (!cap.begin()) {
    Serial.println("MPR121 not found!");
    while(1);  
  }
  Serial.println("MPR121 found, continuing");
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
      Serial.println( "V_LIGHT command received..." );
  
      int lstate= atoi( message.data );
      if ((lstate<0)||(lstate>1)) {
        Serial.println( "V_LIGHT data invalid (should be 0/1)" );
        return;
      }
      currentState=lstate;
      saveState(EPROM_LIGHT_STATE, currentState);
  
      if ((currentState==LIGHT_ON) && (desiredLevel==0)) {
        //In the case that the Light State = On, but the dimmer value is zero,
        //then something (probably the controller) did something wrong,
        //for the Dim value to 100%
        desiredLevel=50;
        saveState(EPROM_DIMMER_LEVEL, desiredLevel);
      }
      break;
    }
    case V_PERCENTAGE: {
      Serial.println( "V_DIMMER command received..." );
      int dimvalue= atoi( message.data );
      if ((dimvalue<0) || (dimvalue>100)) {
        Serial.println( "V_DIMMER data invalid (should be 0..100)" );
        return;
      }
      if (dimvalue==0) {
        currentState=LIGHT_OFF;
      } else {
        currentState=LIGHT_ON;
        desiredLevel=dimvalue;
        saveState(EPROM_DIMMER_LEVEL, desiredLevel);
      }
      break;
    }
    default:
      Serial.println( "Invalid command received..." );
      return;
  }
}

void loop()
{
  static unsigned long touchActivatedMillis;
  static unsigned long touchReleasedMillis;
  static unsigned long touchDuration;
  static unsigned long noTouchDuration;
  static unsigned long nextDimming;
  static uint16_t lastTouch;
  uint16_t currentTouch = cap.touched();
  
  if (currentTouch == 0) {
    touchActivatedMillis = millis();
    touchDuration = 0;
    noTouchDuration = millis() - touchReleasedMillis;
  } else {
    touchDuration = millis() - touchActivatedMillis;
    touchReleasedMillis = millis();
    if (lastTouch == 0) {
      if (dimmerDirection == UP) {
        dimmerDirection = DOWN;
      } else {
        dimmerDirection = UP;
      }
    }
  }
  
  if (
    // (currentTouch & DIMMER_TOUCH_MASK) 
    // && 
    (noTouchDuration > TOUCH_TIME_BETWEEN_ACTIONS) 
  ) {
    if (nextDimming < millis()) {
      if (touchDuration > TOUCH_TIME_DIMMING) {
        handleDimming();
      }
      else if (touchDuration < TOUCH_TIME_DIMMING && touchDuration > TOUCH_TIME_TOGGLE_POWER) {
        handlePowerToggle();
      }
      nextDimming = millis() + 100;
    }
  }
  
  if (currentTouch != lastTouch) {
    Serial.print("Touched : ");
    Serial.print(currentTouch);
    Serial.print(" - Duration : ");
    Serial.print(touchDuration);
    Serial.print(" - Sleep : ");
    Serial.println(noTouchDuration);
    lastTouch = currentTouch;
  }
  fadeToDesiredLevel();
  SendCurrentState2Controller();
}

/**
 * Handle touch action for lights on / off
 */
void handlePowerToggle() {
  Serial.println("Power Toggle");
  currentState = !currentState;
}

/**
 * Handle touch action for dimming of lights (up / down)
 */
void handleDimming() {

  // Turn light on
  if (currentState == LIGHT_OFF) {
    currentState = LIGHT_ON;
  }

  // Dimming
  if (dimmerDirection == UP) {
    Serial.println("UP");
    desiredLevel+=2;
  } else {
    Serial.println("DOWN");
    desiredLevel-=2;
  }

  // Change dimming direction
  if (desiredLevel <= 0) {
    dimmerDirection = UP;
    desiredLevel = 0;
    Serial.println("ChangeDimmer UP");
  }
  if (desiredLevel >= 100) {
    desiredLevel = 100;
    dimmerDirection = DOWN;
    Serial.println("ChangeDimmer down");
  }
}

/***
 *  This method provides a graceful fade up/down effect
 */
void fadeToDesiredLevel()
{
  static unsigned long nextChange;
  static int16_t currentLevel;
  if (nextChange < millis()) {
    nextChange = millis() + FADE_DELAY;

    int target = desiredLevel;
    if (currentState == LIGHT_OFF) {
      target = 0;
    }
    
    if (currentLevel != target ) {
      int delta = ( target - currentLevel ) < 0 ? -1 : 1;      
      currentLevel += delta;
      analogWrite( LED_PIN1, (int)(currentLevel * 2.55) );
    }
  }
}

/***
 * Send desired level to mysensors controller, if changed from last time it was sent
 */
void SendCurrentState2Controller()
{
  static int16_t lastTransmittedLevel;
  int16_t level = desiredLevel;
  if (currentState == LIGHT_OFF) {
    level = 0;
  }
  
  if (lastTransmittedLevel != level) {
    send(dimmerMsg.set((int16_t)level));
    lastTransmittedLevel = level;
  }
}
