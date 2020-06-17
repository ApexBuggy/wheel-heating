#include <Wire.h>                  // I2C library, required for MLX90614
#include <SparkFunMLX90614.h>      // IR Temperature Sensor
#include <RBDdimmer.h>             // Dimmer Module
#include <LiquidCrystal_PCF8574.h> // LCD Module

/*
 * TODO:
 */

// Error handling
// Status LEDs, or LCD status
// Way to lock temperatures so dials don't accidentally move
// "Minimum off time" for heater so it doesn't spam on/off

/*
 * Configuration
 */

const int NUM_WHEELS = 3;
// If you change this parameter (particularly to increase it), be sure
// to change ALL the initializer lists declared with length NUM_WHEELS
// to match the new value. If NUM_WHEELS is greater than the length of
// a initializer list, some elements will default to 0.

// Minimum and maximum settings for the heater (Fahrenheit).
// This is the same as the full range of the potentiometer.
// Careful that this range does not exceed min/max_temp below.
const int TEMP_MAP_LO = 50;
const int TEMP_MAP_HI = 150;

// Pins and addrs for wheel             A,    B,    C
const int DIMMER_PINS[NUM_WHEELS] = {   3,    4,    5};
const int DIAL_PINS[NUM_WHEELS]   = {  A0,   A1,   A2};
const int THERM_ADDRS[NUM_WHEELS] = {0x5A, 0x5B, 0x5C};

// How often to update the LCD
const unsigned long LCD_UPDATE_PERIOD = 500; // ms.

// Purely visual. Define a couple custom characters for the LCD to
// indicate whether the heater is on or off.
const int ON_CHAR = 1;
const int OFF_CHAR = 2;
const int ON_CHAR_BITS[8] = { // A little up arrow (heat is going up)
  B00000,
  B00000,
  B01000,
  B11100,
  B00000,
  B00000,
  B00000,
  B00000,
};
const int OFF_CHAR_BITS[8] = { // A little square (heat is "stopped")
  B00000,
  B00000,
  B11100,
  B11100,
  B11100,
  B00000,
  B00000,
  B00000,
};

/*
 * Sanity check parameters
 */

// Operating temperature bounds
const float MIN_TEMP = 20.0;  // deg F. Perhaps this is unreasonably low.
const float MAX_TEMP = 155.0; // deg F. Perhaps this is also unreasonably low.

// How many times we allow reading exactly the same temperature
const int MAX_TEMP_COUNT = 5;

// How long it's sane to go without a new temperature reading
const unsigned long MAX_INTER_TEMP_TIME = 200; // ms. Probably overly generous.

/*
 * Globals
 */

// Declare dimmers, passing each pin to the (implicit) constructor
dimmerLamp dimmers[NUM_WHEELS] = {DIMMER_PINS[0], DIMMER_PINS[1],
                                  DIMMER_PINS[2]};

// Declare thermometers. The constructor doesn't take an argument,
// so we don't need the array as above.
IRTherm therms[NUM_WHEELS];

// Declare LCD at I2C address 0x27
LiquidCrystal_PCF8574 lcd(0x27);

// Store the target temperatures
float target_temps[NUM_WHEELS];

// Store some information about recent readings for sanity checks.
float prev_temp[NUM_WHEELS];     // Most recent reading
int prev_temp_count[NUM_WHEELS]; // Count of how many consecutive times prev_temp has appeared
unsigned long prev_temp_time[NUM_WHEELS]; // Time of most recent reading

// Time of last LCD update
unsigned long prev_lcd_time;

/*
 * Code
 */

// This function turns off the heaters, prints an error message, and
// infinite loops.

// This is a bad way to handle errors. Depending on the type of error,
// it may be correct to shut off all the heaters, just one heater, or
// perhaps something else entirely. Errors should be reported in some
// better way (probably wheel-specific). For the rest of the heater to
// continue working, this function should not infinite loop.

// This is here to force you to write a better handler. DO NOT simply
// remove the infinite loop and call it good. Be very careful about
// how you approach this, have multiple people review the strategy and
// the code itself, etc. etc. Consider mechanical failures (the wheel
// gets stuck, a wire comes loose), part failures (the servo breaks,
// the temperature sensor breaks), and software failures (the code did
// something wrong). I've tried to identify some common error cases in
// the code, but there are many more. Be sure that error handling code
// doesn't break the rest of the code.

// TODO: replace with a proper error handler
void bad_error_handler() {
  // Disable heaters
  for (int i = 0; i < NUM_WHEELS; i++) {
      dimmers[i].setState(OFF);
  }

  // Print an error message
  lcd.clear();
  lcd.print("ERROR >:(");

  // Never return
  while(1) {
      // Infinite loop
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("Beginnning initialization");

  Serial.print("Initializing LCD... ");
  // TODO: you may need Serial.flush(); after every call to Serial.print
  // if the string does not appear.
  lcd.begin(16, 2);      // 16 cols x 2 rows
  lcd.setBacklight(255); // Enable backlight
  lcd.noCursor();
  lcd.createChar(ON_CHAR, ON_CHAR_BITS); // Create the custom characters
  lcd.createChar(OFF_CHAR, OFF_CHAR_BITS);
  Serial.println("done.");

  // Initialize each wheel
  for (int i = 0; i < NUM_WHEELS; i++) {
    Serial.print("Initializing wheel #");
    Serial.println(i);

    Serial.print("  Initializing dimmer... ");
    dimmers[i].begin(NORMAL_MODE, OFF);
    dimmers[i].setPower(1);
    Serial.println("done.");

    Serial.print("  Initializing thermometer... ");
    therms[i].begin(THERM_ADDRS[i]);
    therms[i].setUnit(TEMP_F); // Fahrenheit, I guess
    Serial.println("done.");

    target_temps[i] = 0.0; // Will be overwritten in the loop

    prev_temp[i] = 0.0; // Doesn't matter much
    prev_temp_count[i] = 0;
    prev_temp_time[i] = millis();

    pinMode(DIAL_PINS[i], INPUT);
    Serial.println("done.");
  }

  Serial.println("Initialization complete");
}

void loop() {
  for (int i = 0; i < NUM_WHEELS; i++) {
    // Read the potentiometer for target temp
    target_temps[i] = (float)(map(analogRead(DIAL_PINS[i]),   // Map the sensor reading
                                  0, 1023,                    // From analogRead's range
                                  TEMP_MAP_LO, TEMP_MAP_HI)); // To our setting range

    // Check if a new reading is available for this sensor
    if (therms[i].read()) {
      prev_temp_time[i] = millis();

      // And get that reading (in Fahrenheit)
      float temp = therms[i].object();
      float ambient_temp = therms[i].ambient();

      if (temp == prev_temp[i]) { // Generally == on floats is bad, but
                                  // here we want an exact comparison
        prev_temp_count[i]++;
      } else {
        prev_temp[i] = temp;
        prev_temp_count[i] = 1;
      }

      // Sanity check the readings
      if (temp < MIN_TEMP || temp > MAX_TEMP) {
        // Object temp out of range
        // TODO error
        bad_error_handler();

      }

      if (ambient_temp < MIN_TEMP || ambient_temp > MAX_TEMP) {
        // Ambient temp out of range
        // TODO error
        bad_error_handler();

      }

      if (prev_temp_count[i] > MAX_TEMP_COUNT) {
        // Temperature exactly the same for too long
        // TODO error
        bad_error_handler();

      }

      // Change the heat
      // This currently does the dumbest possible thing:
      // Turn the heat on if the sensor reads cold, turn the heat off if it reads hot.
      // This does not use the "dimmer" function of the dimmer at all - just on or off.
      //  Even on the lowest setting, the dimmer seems to be too hot, though that may
      //  be user error.
      // More sophisticated approaches like PID need some kind of analog control.
      //  The dimmer would achieve this, but you could also do something like turn the
      //  heater on for a variable fraction of every second.

      bool cur_state = dimmers[i].getState();
      if (cur_state == ON && temp > target_temps[i]) {
        // Heat is on but the temperature is too high
        dimmers[i].setState(OFF);
      } else if (cur_state == OFF && temp < target_temps[i]) {
        // Heat is off but the temperature is too low
        dimmers[i].setState(ON);
      }

    } else {
      // Sanity check time if new reading is unavailable
      if (millis() > prev_temp_time[i] + MAX_INTER_TEMP_TIME) {
        // No new reading for too long
        // TODO error
        bad_error_handler();

      }

    }
  }

  // Update the LCD
  unsigned long cur_time = millis();
  if (cur_time > prev_lcd_time + LCD_UPDATE_PERIOD) {
    prev_lcd_time = cur_time;

    // One row is 16 chars plus one for the NUL terminator
    char lcd_row[16+1];

    char *msg_ptr = lcd_row;

    // Take care, as this does not clear the display (that causes a bit of flashing).
    // Instead, this makes sure to write something (character or space) to the same
    // characters on the screen every time.
    // lcd.clear();
    lcd.home(); // Move cursor to the upper-left corner

    for (int i = 0; i < NUM_WHEELS; i++) {
      // Fill buffer with recent temp reading,
      pad_to_3(prev_temp[i], msg_ptr);
      msg_ptr += 3;
      // then '/',
      *msg_ptr = '/';
      msg_ptr += 1;
      // then target temp,
      pad_to_3(target_temps[i], msg_ptr);
      msg_ptr += 3;
      // then an indicator of whether the heater is on or off.
      *msg_ptr = (dimmers[i].getState() ? ON_CHAR : OFF_CHAR);
      msg_ptr += 1;

      // Print and reset when either a row is filled (exactly 2 entries)
      // or this is the last entry
      if ((i+1) % 2 == 0 || (i+1) == NUM_WHEELS) {
        // NUL terminate the string
        *msg_ptr = '\0';

        Serial.print(lcd_row);
        lcd.print(lcd_row);

        msg_ptr = lcd_row;

        // Once we're going to the second row, move the cursor down
        if ((i+1) / 2 == 1) {
          lcd.setCursor(0,1);
        }
      }
    }

    // Print a newline
    Serial.println();
  }
}

// LCD screen layout
//  ----------------
// |135/140^142/140.| Except ^ and . are ON_CHAR and OFF_CHAR, see above.
// |130/130.139/145^|
//  ----------------

// Convert a float to a string of length 3 starting at buf
void pad_to_3(float f, char *buf) {
  int i = (int)f;
  if (i >= 1000) {
    strcpy(buf, "OVF"); // Overflow
    return;
  }
  if (i < 0) {
    strcpy(buf, "UNF"); // Underflow
    return;
  }

  if (i < 100) {
    // 1- and 2-digit numbers should skip the first character, filling with space
    buf[0] = ' ';
    buf++;
  }

  if (i < 10) {
    // 1-digit number should skip another character, filling with space
    buf[0] = ' ';
    buf++;
  }

  // Convert int to string in base 10
  itoa(i, buf, 10);
}
