/*********************************
*   LED Array Fireplace Effect
*   Author: kvkontin@gmail.com
*   2024
**********************************/

/*
*   Uses PWM capable pins on an Arduino compatible board
*   to simulate the look of a flame fading to embers.
*   Attach a red LED to the designated red pin,
*   followed by some number of yellow LEDs in a line.
*   A push button can be used to increase the remaining
*   burn time with a short press, or decrease by holding.
*   For a moment after each input, burn time is shown by
*   fully lighting a fraction of the LEDs.
*/

// Pin indices
const int RED_PIN = 3;
const int YELLOW_COUNT = 5;
const int YELLOW_PINS[] = {5,6,9,10,11};
const int BUTTON_PIN = 12;

// User interaction properties
const int SHORT_PRESS_MINIMUM = 30; // milliseconds
const int LONG_PRESS_MINIMUM = 200; // milliseconds
const int MENU_MODE_EXIT = 2000; // milliseconds
const float EXTINGUISH_RATE = 0.0005; // unitless

// Flame simulation properties
const int WAVE_COUNT = 3; // number of sinusoid waves
const float WAVELENGTHS[] = {0.8391, 0.59223, 0.44551}; // in lengths of LED array
const float FLOW_RATES[] = {0.741294, 1.238127, 2.3836745}; // in lengths of LED array per second
const float AMPLITUDES[] = {0.2, 0.12, 0.089}; // 1.0 = full brightness range of LED
const float WAVE_BIAS = 0.4; // 1.0 = full brightness range of LED
const float BURN_TIME = 2.0; // in hours

// Computed properties, do not edit
const unsigned long FIRE_LIFE_EXPECTANCY = 1000L * 60 * 60 * BURN_TIME; // milliseconds
const float PIN_DISTANCE = 1.0/YELLOW_COUNT;
unsigned long fire_life_spent = 0; // milliseconds
unsigned long fire_life_before_edit = 0; // milliseconds
float fire_life_progress = 0; // unitless
unsigned long last_ms = 0; // milliseconds
float flame_vigor = 1.0;
float ember_brightness = 0.0;
bool menu_mode = false;
bool button_was_pressed = false;
unsigned long button_change_time = 0;
unsigned long menu_mode_entry_time = 0;

// Maps values from [0..1] to [0..255], with clamping
int signedToPWM(float f) {
  float clamped = max(0.0,min(1.0,f));
  return 255*clamped;
}

void setup() {
  pinMode(RED_PIN, OUTPUT);
  for(int index = 0; index < YELLOW_COUNT; index++) {
    pinMode(YELLOW_PINS[index], OUTPUT);
  }
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void loop() {
  // Update timing variables
  unsigned long ms = millis();
  float seconds = ms/1000.0;

  // Respond to button presses
  if(digitalRead(BUTTON_PIN) == LOW) { // Button is currently held
    if(!button_was_pressed) { // Button was not held before
      button_was_pressed = true;
      button_change_time = ms;
      fire_life_before_edit = fire_life_spent;
    }
    else { // Button has been held for several ticks
      menu_mode = true;
      menu_mode_entry_time = ms;
      // As a long press continues, gradually extinguish fire
      if(ms-button_change_time > LONG_PRESS_MINIMUM) {
        fire_life_spent += EXTINGUISH_RATE * FIRE_LIFE_EXPECTANCY;
        if(fire_life_spent > FIRE_LIFE_EXPECTANCY) {
          fire_life_spent = FIRE_LIFE_EXPECTANCY;
        }
      } // End of "long press continues"
    } // End of "held for several ticks"
  } // End of "currently held"

  else { // Button is not currently held
    if(button_was_pressed) {
      // Button has just been released
      // Detect the length of the hold
      int button_hold_duration = ms-button_change_time;

      if(button_hold_duration > SHORT_PRESS_MINIMUM
        && button_hold_duration < LONG_PRESS_MINIMUM) {
        // If already in menu mode, rewind the lifetime of the fire
        if(menu_mode) {
          unsigned long increment = (1.0*FIRE_LIFE_EXPECTANCY/YELLOW_COUNT);
          if(fire_life_spent < increment) {fire_life_spent = 0;}
          else {fire_life_spent -= increment;}
        }
        // Go to menu mode on any input
        menu_mode = true;
        menu_mode_entry_time = ms;
      } // End of short press
    }
    button_was_pressed = false;
    button_change_time = ms;
  } // End of "not currently held"

  // Update fire progress fraction, needed for menu
  fire_life_progress = (1.0*fire_life_spent / FIRE_LIFE_EXPECTANCY);

  // Set LED brightnesses

  if(menu_mode) {
      // Show progress bar
      digitalWrite(RED_PIN, HIGH);
      for(int index = 0; index < YELLOW_COUNT; index++) {
        bool progress_bar_on = (index < (1.0-fire_life_progress)*YELLOW_COUNT);
        digitalWrite(YELLOW_PINS[index], progress_bar_on ? HIGH : LOW);
      }
    if(ms-menu_mode_entry_time > MENU_MODE_EXIT) {
      menu_mode = false;
    }
  } // End of menu mode

  else { // Not menu mode, show the fire

    // Fade the fire out and the ember in as flame burns down
    fire_life_spent += ms-last_ms;
    fire_life_progress = (1.0*fire_life_spent / FIRE_LIFE_EXPECTANCY);
    last_ms = ms;
    flame_vigor = constrain(1.0-fire_life_progress/0.75, 0, 1);
    ember_brightness = constrain(1.0-4.0*abs(fire_life_progress-0.75), 0, 1);

    // Sum up the sinusoid waves for instantaneous brightness of LEDs
    for(int led_index = 0; led_index < YELLOW_COUNT; led_index++) {
      float summed_waves = WAVE_BIAS;
      float led_vs_world = PIN_DISTANCE*led_index; // 0..1
      float length_fraction = 1.0*led_index/YELLOW_COUNT;

      for(int wave_index = 0; wave_index < WAVE_COUNT; wave_index++) {
        float wave_vs_world = FLOW_RATES[wave_index]*seconds;
        float led_vs_wave = led_vs_world-wave_vs_world;
        float wave_cycles = led_vs_wave/WAVELENGTHS[wave_index];
        float sine = AMPLITUDES[wave_index]*sin(2*PI*wave_cycles);
        summed_waves += sine;
      } // End of "sum up waves"

      // Apply fadeout multiplier
      summed_waves *= flame_vigor;

      // Output flame as PWM
      int out = signedToPWM(summed_waves);
      analogWrite(YELLOW_PINS[led_index], out);
    }
    // Output ember glow as PWM
    analogWrite(RED_PIN, signedToPWM(ember_brightness));

  } // End of "not menu mode"

  // Sleep between computations to save power
  delay(2);
  // If fire is completely out, sleep longer
  if(fire_life_progress >= 1.0) {
    delay(50);
  }
}