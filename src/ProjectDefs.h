#ifndef ProjectDefs_h
#define ProjectDefs_h
#include <Arduino.h>

static const uint8_t LED_PIN    = 6; // PWM output for LED strip
static const uint8_t IRQ_PIN    = 3; // Interrupt from mpr121 (not currently used though)


// Max level we are allowing for dimming (100% on)
const long DIMMING_MAX_LEVEL = 100;
// Number of millis a full span fade should take (both when getting new level from remote, and local by touch)
const long FADE_SPEED = 2000;
 
// Minimum time for turning light on / off
const long TOUCH_TIME_TOGGLE_POWER = 100;
// Minimum time before turning light level up or down
const long TOUCH_TIME_DIMMING = 500;
// Minimum time between touch actions (no touch duration)
const long TOUCH_TIME_BETWEEN_ACTIONS = 200;



// Calculated values for determining transition speed
const long FADE_DELAY = FADE_SPEED / DIMMING_MAX_LEVEL;

#endif

