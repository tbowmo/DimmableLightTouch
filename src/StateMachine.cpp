#include <Arduino.h>
#include "StateMachine.h"

// definition of the state machine : state & properties
typedef struct {
  StateDefinition* currentState;
  uint32_t stateEnter;
} dimmerSM;

static dimmerSM _dimmerSM;
static int16_t desiredLightLevel;
static int16_t currentLightLevel;
static bool currentLightState;
static bool dimmerUp;

/**
 * Internal method declarations
 */
bool setNewLevel(int16_t newValue);
void (*_sendStateCallback)(int16_t, bool);

bool CurrentStateIs(StateDefinition& state) {
  return _dimmerSM.currentState ==  &state;
}

uint32_t TimeInState() {
  return millis() - _dimmerSM.stateEnter;
}

void SwitchSM(StateDefinition& newState) {
  Serial.print(millis());
  Serial.print("# ");
  Serial.print(newState.name);
  Serial.print(", ");
  Serial.print(_dimmerSM.currentState->name);
  Serial.print(", ");
  Serial.println(TimeInState());

  // Change state if needed
  if (_dimmerSM.currentState != &newState) _dimmerSM.currentState = &newState;
  // Transition event
  if (_dimmerSM.currentState->Transition) _dimmerSM.currentState->Transition();
  // save time
  _dimmerSM.stateEnter = millis();
}

/********************************************************************
 * states / state transitions below here, should not be called outside
 * the statemachine.
 */
void StateTouchBeginTransition();
void StateTouchBegin();
void StateTouchEndTransition();
void StateTouchDimmer();
void StateControllerValue();
void StateIdleTransition();

/**********************************/
static StateDefinition sdTouchBegin      = { StateTouchBeginTransition, StateTouchBegin, "Touch start"};
static StateDefinition sdTouchEnd        = { StateTouchEndTransition, NULL, "Touch end"};
static StateDefinition sdDimmer          = { NULL, StateTouchDimmer, "Dimmer"};
static StateDefinition sdControllerValue = { NULL, StateControllerValue, "Controller value"};
static StateDefinition sdIdle            = { StateIdleTransition, NULL, "Idle"};
/**********************************/

/********* States *************/
void StateTouchBeginTransition() {
  if (TimeInState() < TOUCH_TIME_BETWEEN_ACTIONS) {
    SwitchSM(sdIdle);
  }
  dimmerUp = !dimmerUp;
}

void StateTouchBegin() {
  if (TimeInState() > TOUCH_TIME_DIMMING) {
    SwitchSM(sdDimmer);
  }
}

void StateTouchEndTransition() {
  if (TimeInState() < TOUCH_TIME_DIMMING) {
    currentLightState = !currentLightState;
    SwitchSM(sdControllerValue);
  } else {
    SwitchSM(sdIdle);
  }
}

void StateTouchDimmer() {
  if ((TimeInState() % FADE_DELAY) == 0) {
    char direction = dimmerUp ? + 1 : -1;
    bool result = setNewLevel(currentLightLevel + direction);
    if (!result) {
      dimmerUp = !dimmerUp;
      Serial.print("Change direction ");
    }
    Serial.println(dimmerUp ? "UP" : "DOWN");
  }
}

void StateControllerValue() {
  int16_t level = desiredLightLevel;
  if (!currentLightState) {
    level = 0;
  }

  if (currentLightLevel != level) {
    if ((TimeInState() % FADE_DELAY) == 0) {
      char direction = (currentLightLevel < level) ? + 1 : -1;
      setNewLevel(currentLightLevel + direction);
    }
  } else { // if we reached correct light level, then jump to idle mode.
    SwitchSM(sdIdle);
  }
}

void StateIdleTransition() {
  _sendStateCallback(currentLightLevel, currentLightState);
  if (currentLightState) {
    desiredLightLevel = currentLightLevel;
  }
}

/**
 * Set new value, with max / min checks
 * returns true if newValue is within allowed range
 */
bool setNewLevel(int16_t newValue) {
  currentLightLevel = constrain(newValue, 0, DIMMING_MAX_LEVEL);

  analogWrite(LED_PIN, (int)(currentLightLevel * 2.55));

  return currentLightLevel == newValue;
}

/**
 * Public interfaces
 **/

void touch(bool state) { // true = touch, false = no touch
  if (state)
    SwitchSM(sdTouchBegin);
  else
    SwitchSM(sdTouchEnd);
}

void controllerValue(int16_t newValue) {
  int16_t constrainedValue = constrain(newValue, 0, DIMMING_MAX_LEVEL);

  if (constrainedValue != currentLightLevel) {
    if (constrainedValue > 0) {
      currentLightState = true;
      desiredLightLevel = constrainedValue;
    } else {
      currentLightState = false;
    }
    SwitchSM(sdControllerValue);
  }
}

void controllerOnOff(bool newState) {
  if (newState != currentLightState) {
    if (newState && (currentLightLevel==0)) {
      //In the case that the Light State = On, but the dimmer value is zero,
      //then something (probably the controller) did something wrong,
      //force the Dim value to 100%
      desiredLightLevel = DIMMING_MAX_LEVEL;
    }
    currentLightState = newState;
    SwitchSM(sdControllerValue);
  }
}

void InitSM(void(*sendCallback)(int16_t, bool), int16_t initialValue) {
  _sendStateCallback = sendCallback;
  currentLightLevel = 0;
  desiredLightLevel = initialValue;
  currentLightState = initialValue != 0;
  SwitchSM(sdControllerValue);
}

void UpdateSM() {
  if (_dimmerSM.currentState->Update) _dimmerSM.currentState->Update();
}
