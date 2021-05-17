#ifndef stateMachine_h
#define stateMachine_h
#include "ProjectDefs.h"

struct StateDefinition {
  void(*Transition)();
  void(*Update)();
  const char* name;
};

void InitSM(void(*alarmCallback)(uint16_t, bool));
void UpdateSM();  // Update the state machine (transition once, then update) etc.

void touch(bool);
void controllerValue(int16_t newValue);
void controllerOnOff(bool);
#endif
