#include <Wire.h>                  // I2C library, required for MLX90614
#include <SparkFunMLX90614.h>      // IR Temperature Sensor
#include <RBDdimmer.h>             // Dimmer Module
#include <LiquidCrystal_PCF8574.h> // LCD Module

/*
 * Configuration
 */

const int NUM_WHEELS = 4;

// Minimum and maximum settings for the heater (Fahrenheit).
// This is the same as the full range of the potentiometer.
// Careful that this range does not exceed min/max_temp below.
const int TEMP_MAP_LO = 50;
const int TEMP_MAP_HI = 150;

// Pins and addrs for wheel   A,    B,    C,    D
const int dimmer_pins[] = {   3,    4,    5,    6};
const int dial_pins[]   = {  A0,   A1,   A2,   A3};
const int therm_addrs[] = {0x5A, 0x5B, 0x5C, 0x5D};

// How often to update the LCD
const unsigned long lcd_update_period = 500; // ms.

// Purely visual. Define a couple custom characters for the LCD to
// indicate whether the heater is on or off.
const int on_char = 1;
const int off_char = 2;
int on_char_bits[8] = { // A little up arrow
  B00000,
  B00000,
  B01000,
  B11100,
  B00000,
  B00000,
  B00000,
  B00000,
};
int off_char_bits[8] = { // A little square
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
const float min_temp = 20.0;  // deg F. Perhaps this is unreasonably low.
const float max_temp = 155.0; // deg F. Perhaps this is also unreasonably low.

// How many times we allow reading exactly the same temperature
const int max_temp_count = 5;

// How long it's sane to go without a new temperature reading
const unsigned long max_inter_temp_ms = 200; // ms. Probably overly generous.

/*
 * Globals
 */

// Declare dimmers, passing each pin to the (implicit) constructor
dimmerLamp dimmers[] = {dimmer_pins[0], dimmer_pins[1],
                        dimmer_pins[2], dimmer_pins[3]};

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

void setup() {
  Serial.begin(9600);

  // Initialize LCD
  lcd.begin(16, 2);      // 16 cols x 2 rows
  lcd.setBacklight(255); // Enable backlight
  lcd.noCursor();
  lcd.createChar(on_char, on_char_bits); // Create the custom characters
  lcd.createChar(off_char, off_char_bits);

  // Initialize each wheel
  for (int i = 0; i < NUM_WHEELS; i++) {
    dimmers[i].begin(NORMAL_MODE, OFF);
    dimmers[i].setPower(1);

    // Initialize the thermometer at the given 7-bit address
    therms[i].begin(therm_addrs[i]);
    therms[i].setUnit(TEMP_F); // Fahrenheit, I guess

    target_temps[i] = 0.0; // Will be overwritten in the loop

    prev_temp[i] = 0.0; // Doesn't matter much
    prev_temp_count[i] = 0;
    prev_temp_time[i] = millis();

    pinMode(dial_pins[i], INPUT);
  }

  // TODO:
  // Status LED/LEDs?
  // Something to indicate if heater is currently on
  // Way to lock temperatures so dials don't accidentally move
  // "Minimum off time" for heater
  // Wiring diagram
}

void loop() {
  for (int i = 0; i < NUM_WHEELS; i++) {
    // Read the potentiometer for target temp
    target_temps[i] = (float)(map(analogRead(dial_pins[i]),   // Map the sensor reading
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
      if (temp < min_temp || temp > max_temp) {
        // Object temp out of range
        // TODO error
        
      }
      
      if (ambient_temp < min_temp || ambient_temp > max_temp) {
        // Ambient temp out of range
        // TODO error
        
      }

      if (prev_temp_count[i] > max_temp_count) {
        // Temperature exactly the same for too long
        // TODO error
        
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
      if (millis() > prev_temp_time[i] + max_inter_temp_ms) {
        // No new reading for too long
        // TODO error
        
      }
      
    }
  }

  // Update the LCD
  unsigned long cur_time = millis();
  if (cur_time > prev_lcd_time + lcd_update_period) {
    prev_lcd_time = cur_time;

    // One row is 16 chars plus one for the NUL terminator
    char lcd_row[16+1];
    // NUL terminate the string
    lcd_row[sizeof(lcd_row) - 1] = '\0';

    char *msg_ptr = lcd_row;

    // Take care, as this does not clear the display (that causes a bit of flashing).
    // Instead, this makes sure to write something (number or space) to every character
    // on the screen every time.
    // lcd.clear();
    lcd.home();

    for (int i = 0; i < NUM_WHEELS; i++) {
      // Fill buffer with recent temp reading,
      pad_to_3(139., msg_ptr);
//      pad_to_3(prev_temp[i], msg_ptr);
      msg_ptr += 3;
      // then '/',
      *msg_ptr = '/';
      msg_ptr += 1;
      // then target temp,
      pad_to_3(target_temps[i], msg_ptr);
      msg_ptr += 3;
      // then an indicator of whether the heater is on or off.
      *msg_ptr = (dimmers[i].getState() ? on_char : off_char);
      msg_ptr += 1;

      // Two entries fill the row. Print and reset.
      if ((i+1) % 2 == 0) {
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
// |135/140^142/140.| Except ^ and . are on_char and off_char, see above.
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
