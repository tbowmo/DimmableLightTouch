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

// Enable debug prints
#define MY_DEBUG

// Enable and select radio type attached
#define MY_RADIO_NRF24
//#define MY_RADIO_RFM69
#define MY_REPEATER_FEATURE

#include <MySensors.h>
#include <Adafruit_MPR121.h>
#include <Wire.h>

#define CHILD_ID_LIGHT 1
#define CHILD_ID_KEY 3

#define EPROM_LIGHT_STATE 1
#define EPROM_DIMMER_LEVEL 2

#define LIGHT_OFF 0
#define LIGHT_ON 1

#define SN "Dimable Light-touch"
#define SV "1.0"

#define LED_PIN1 6
#define LED_PIN2 5
#define FADE_DELAY 10  // Delay in ms for each percentage fade up/down (10ms = 1s full-range dim)

static int16_t currentLevel = 0;  // Current dim level...

int16_t LastLightState=LIGHT_OFF;
int16_t LastDimValue=50;

const int irqPin = 3;
uint16_t currtouched = 0;
uint16_t lasttouched = 0;

uint16_t dimmerTouchMask = 2047;

unsigned long timer;
unsigned long timeDiff;

const long SHORTPRESS = 400;
const long MINPRESS = 50;

bool DimmerDir = false;

Adafruit_MPR121 cap = Adafruit_MPR121();


MyMessage dimmerMsg(CHILD_ID_LIGHT, V_DIMMER);

void setup()
{
  Serial.begin(115200);
  pinMode(irqPin, INPUT);
  digitalWrite(irqPin, HIGH);
  
  pinMode(LED_PIN1, OUTPUT);
  pinMode(LED_PIN2, OUTPUT);
  analogWrite(LED_PIN1, 0);
  analogWrite(LED_PIN2, 0);
  //Retreive our last light state from the eprom
	int LightState=loadState(EPROM_LIGHT_STATE);
	if (LightState<=1) {
		LastLightState=LightState;
		int DimValue=loadState(EPROM_DIMMER_LEVEL);
		if ((DimValue>0)&&(DimValue<=100)) {
			//There should be no Dim value of 0, this would mean LIGHT_OFF
			LastDimValue=DimValue;
		}
	}
 
	SetCurrentState2Hardware();

	Serial.println( "Node ready to receive messages..." );

  LastLightState = LIGHT_OFF;  

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

void loop()
{
  currtouched = cap.touched();

  if (lasttouched == 0) {
    timer = millis();
    if (DimmerDir) {
      DimmerDir = false;
    } else {
      DimmerDir = true;
    }
    timeDiff = 0;
  } else {
    timeDiff = millis() - timer;
  }
  
  if (currtouched & dimmerTouchMask) {
    Serial.println(DimmerDir);
    if (timeDiff > SHORTPRESS) {
      if (LastLightState == LIGHT_OFF) {
        LastLightState = LIGHT_ON;
      }
      Serial.print("Dimmer ");
      if (DimmerDir) {
        Serial.println("UP");
        LastDimValue+=2;
      } else {
        Serial.println("DOWN");
        LastDimValue-=2;
      }
      if (LastDimValue <= 0) {
        DimmerDir = true;
        LastDimValue = 0;
        Serial.println("ChangeDimmer UP");
      }
      if (LastDimValue >= 100) {
        LastDimValue = 100;
        DimmerDir = false;
        Serial.println("ChangeDimmer down");
      }
      SetCurrentState2Hardware();
      wait(50);
    }
  }
  else {
    if (lasttouched & dimmerTouchMask) {
      if (timeDiff < SHORTPRESS && timeDiff > MINPRESS) {
        Serial.println("Power Toggle");
        ledChangePower();
      }
    }
  }
  Serial.print("Touched : ");
  Serial.println(currtouched);

  lasttouched = currtouched; 
}

void ledChangePower() {
  if (LastLightState == LIGHT_ON) {
    LastLightState = LIGHT_OFF;
  } else {
    LastLightState = LIGHT_ON;
  }
  SetCurrentState2Hardware();
}

void receive(const MyMessage &message)
{
	if (message.type == V_LIGHT) {
		Serial.println( "V_LIGHT command received..." );

		int lstate= atoi( message.data );
		if ((lstate<0)||(lstate>1)) {
			Serial.println( "V_LIGHT data invalid (should be 0/1)" );
			return;
		}
		LastLightState=lstate;
		saveState(EPROM_LIGHT_STATE, LastLightState);

		if ((LastLightState==LIGHT_ON)&&(LastDimValue==0)) {
			//In the case that the Light State = On, but the dimmer value is zero,
			//then something (probably the controller) did something wrong,
			//for the Dim value to 100%
			LastDimValue=50;
			saveState(EPROM_DIMMER_LEVEL, LastDimValue);
		}

		//When receiving a V_LIGHT command we switch the light between OFF and the last received dimmer value
		//This means if you previously set the lights dimmer value to 50%, and turn the light ON
		//it will do so at 50%
	} else if (message.type == V_DIMMER) {
		Serial.println( "V_DIMMER command received..." );
		int dimvalue= atoi( message.data );
		if ((dimvalue<0)||(dimvalue>100)) {
			Serial.println( "V_DIMMER data invalid (should be 0..100)" );
			return;
		}
		if (dimvalue==0) {
			LastLightState=LIGHT_OFF;
		} else {
			LastLightState=LIGHT_ON;
			LastDimValue=dimvalue;
			saveState(EPROM_DIMMER_LEVEL, LastDimValue);
		}
	} else {
		Serial.println( "Invalid command received..." );
		return;
	}

	//Here you set the actual light state/level
	SetCurrentState2Hardware();
}

void SetCurrentState2Hardware()
{
	if (LastLightState==LIGHT_OFF) {
		Serial.println( "Light state: OFF" );
    fadeToLevel(0);
	} else {
		Serial.print( "Light state: ON, Level: " );
    fadeToLevel(LastDimValue);
		Serial.println( LastDimValue );
	}

	//Send current state to the controller
	SendCurrentState2Controller();
}

void SendCurrentState2Controller()
{
	if ((LastLightState==LIGHT_OFF)||(LastDimValue==0)) {
		send(dimmerMsg.set((int16_t)0));
	} else {
		send(dimmerMsg.set(LastDimValue));
	}
}

/***
 *  This method provides a graceful fade up/down effect
 */
void fadeToLevel( int toLevel )
{

  int delta = ( toLevel - currentLevel ) < 0 ? -1 : 1;

  while ( currentLevel != toLevel ) {
    currentLevel += delta;
    analogWrite( LED_PIN1, (int)(currentLevel / 100. * 255) );
    analogWrite( LED_PIN2, (int)(currentLevel / 100. * 255) );
    wait( FADE_DELAY );
  }
}



