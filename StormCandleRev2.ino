
// StormCandle Rev.2
// - For Ann -

// Displays the relative air pressure on a red (high) - blue (low) scale
// Self-learning, remembers the highest and lowest pressure it ever encountered
// Please perform a reset after significant elevation changes

// To reset the min/max values:
// 1. Power off the Arduino
// 2. Connect Pin 12 to ground (hold down the reset button)
// 3. Power on the Arduino
// 4. The LED will blink red three times
// 5. After one final green blink (2 seconds), the min/max values will be reset to just above and below the current measurement
// Note: With each reset, the memory addresses will be rotated through the EEPROM to reduce wear

// Connect the adressable RGBW LED to GND, to +5V and to PIN_LED

// Connect Arduino Nano -> pressure sensor BMP180
// A5  -> SCL
// A4  -> SDA
// GND -> GND
// VIN -> 3.3V

// EEPROM layout: (one letter = one byte)
// XXAAAABBBBCCCCDDDD
// I.L...L...L...L... (I = integer, L = long)
// XX   = Memory address
// AAAA = Minimum pressure at first run
// BBBB = Maximum pressure at first run
// CCCC = Minimum pressure after first reset
// DDDD = Maximum pressure after first reset
// etc.


#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_BMP085.h>
#include <Adafruit_NeoPixel.h>

Adafruit_BMP085 bmp;

//#define DEBUG 1

#define PIN_LED 6 // D6
#define PIN_RESET_EEPROM 12 // D12
#define PIN_ACCEL_Z A1
#define ACCEL_TRESHOLD 400
#define MEDIAN_COUNT 7 // Number of measurements per loop
#define STARTUP_RANGE 20 // Distance (in Pascale) between initial measurement and maximum / minimum at first startup
#define COLOR_BRIGHTNESS_FACTOR 1 // Make the color LEDs less bright
#define WHITE_BRIGHTNESS_FACTOR 0.1 // Make the white LED less bright

// Misc
int i;
int iMedian;
bool initializing = false;
unsigned long timestamp_last_readout;

bool firstrun = false; //first run;
float cycletime; //cycle time in [ms]
unsigned long micros_last; //last micros() in ms

// Pressures
long pa_measurements[MEDIAN_COUNT]; 
long pa_current;
float pa_current_PT1;
long pa_max;
long pa_min;
long pa_center;

// Manual inputs
bool switch_reset_eeprom;
int  acceleration_z;

// Addresses
int addr_int_addresses_start; // Start of the currently used address space
int addr_long_hpa_min;
int addr_long_hpa_max;
int address_block_end; // The first address after all used addresses

// LED
float led_max = 255;
int   red  = 0;
int   blue = 0;

Adafruit_NeoPixel strip = Adafruit_NeoPixel(1, PIN_LED, NEO_RGBW + NEO_KHZ800);


void setup() {
  setupHardware();

  timestamp_last_readout = millis();

  // First pressure measurement
  iMedian = MEDIAN_COUNT / 2;
  do {
    pa_current = bmp.readPressure();
  } while (pa_current < 900);

  EEPROM.get(0, addr_int_addresses_start);

  // Handle first launch
  if(-1 == addr_int_addresses_start) {
    initializing = true;
    addr_int_addresses_start = sizeof(int); // Leave the first two bytes in EEPROM for the start address of the min/max values
    EEPROM.put(0, addr_int_addresses_start);
  }

  // Check for reset button
  readResetSwitch();

  if(switch_reset_eeprom) {
    for(i = 0; i < 3; i++) {
      setColor(255, 0, 0, 0);
      delay(1000);
      setColor(0, 0, 0, 0);
      delay(1000);
    }

    // Read value again (to be sure)
    readResetSwitch();

    if(switch_reset_eeprom) {
      setColor(0, 255, 0, 0);
      delay(2000);

      // Add up all addresses as they were used before, then start after that. Addresses will be calculated again just below.
      calculateAddresses();
      addr_int_addresses_start = address_block_end;

      // Handle memory address wrap around
      if((E2END - 2*sizeof(long)) <= addr_int_addresses_start) {
        addr_int_addresses_start = sizeof(int);
      }
      
      EEPROM.put(0, addr_int_addresses_start); // Save the memory address to the beginning of the EEPROM memory
      initializing = true;
    }

      setColor(0, 0, 0, 0);
  }

  calculateAddresses();

  if(initializing) {
    #ifdef DEBUG
      Serial.println("Initializing EEPROM with current min/max values");
    #endif
    
    pa_max = pa_current + STARTUP_RANGE;
    pa_min = pa_current - STARTUP_RANGE;
    pa_center = pa_current;
    
    EEPROM.put(addr_long_hpa_min, pa_min);
    EEPROM.put(addr_long_hpa_max, pa_max);
  } else {
    #ifdef DEBUG
      Serial.println("Loading min/max values from EEPROM");
    #endif

    EEPROM.get(addr_long_hpa_min, pa_min);
    EEPROM.get(addr_long_hpa_max, pa_max);
  }

  calculateCenter();
  
  //init cycle time measurement
  micros_last = micros();
  firstrun = true;
}
  
void loop() {
  calcCycleTime();
  
  measureAndCalculatePT1();
  setLed();
  outputPressureOverSerial();

  #ifdef DEBUG
    //debugAddresses();
    //debugButton();
    //debugAcceleration();
    //debugLeds();
    debugPressures();
    Serial.println("");    
  #endif
  firstrun = false;
}

void setupHardware() {
  Serial.begin(9600);
  strip.begin();
  pinMode(PIN_RESET_EEPROM, INPUT_PULLUP);
  
  if (!bmp.begin()) {
    #ifdef DEBUG
      Serial.println("Sensor not found");
    #endif
    
    while (1) {}
  }
}

void calculateAddresses() {
  addr_long_hpa_min = addr_int_addresses_start;
  addr_long_hpa_max = addr_long_hpa_min + sizeof(long);
  address_block_end = addr_long_hpa_max + sizeof(long);
}

void readResetSwitch() {
  switch_reset_eeprom = !digitalRead(PIN_RESET_EEPROM);
  acceleration_z = analogRead(PIN_ACCEL_Z);

  if(acceleration_z > ACCEL_TRESHOLD) {
    switch_reset_eeprom = true;
  }
}

void measureAndCalculate() {
  // Collect measurements
  for (i = 0; i < MEDIAN_COUNT; i++) {
    do {
      pa_measurements[i] = bmp.readPressure();
    } while (pa_measurements[i] < 900);
  }

  // Sort measurements to find the median (center) value(s)
  bubbleSort();
  
  // Average the 3 central values to avoid excess flickering
  pa_current = (pa_measurements[iMedian] + pa_measurements[iMedian - 1] + pa_measurements[iMedian + 1]) / 3;

  // Calculate minimum, maximum and center values
  if(pa_current > pa_max) {
    pa_max = pa_current;
    EEPROM.put(addr_long_hpa_max, pa_max);
  }
  
  if(pa_current < pa_min) {
    pa_min = pa_current;
    EEPROM.put(addr_long_hpa_min, pa_min);
  }
  
  calculateCenter();
}

void calculateCenter() {
  pa_center = pa_min + (pa_max - pa_min) / 2;
}

void setLed() {
  // Set LED colours
  if (pa_current <= pa_center) red = 0; else red = led_max * (pa_current - pa_center) / (pa_max - pa_center);
  if (pa_current >= pa_center) blue = 0; else blue = led_max - (led_max * (pa_current - pa_min) / (pa_center - pa_min));

  setColor(red * COLOR_BRIGHTNESS_FACTOR, 0, blue * COLOR_BRIGHTNESS_FACTOR, led_max * WHITE_BRIGHTNESS_FACTOR);
}

void outputPressureOverSerial() {
  if(millis() - timestamp_last_readout > 60000) {
    timestamp_last_readout = millis();
    Serial.print(pa_current);
    Serial.println("");
  }
}

void setColor(int r, int g, int b, int w) {
  strip.setPixelColor(0, strip.Color(g, r, b, w));
  strip.show();
}

// Copied and adapted from https://www.tigoe.com/pcomp/code/arduinowiring/42/
void bubbleSort() {
  long out, in, swapper;
  
  for (out=0 ; out < MEDIAN_COUNT; out++) {
    for (in=out; in<(MEDIAN_COUNT-1); in++) {
      if ( pa_measurements[in] > pa_measurements[in+1] ) {
        swapper = pa_measurements[in];
        pa_measurements [in] = pa_measurements[in+1];
        pa_measurements[in+1] = swapper;
      }
    }
  }
}

void calcCycleTime() {
  unsigned long micros_current; //current lifetime in [us]
  
  //generate cycle time  
  micros_current = micros();
  if (micros_current >= micros_last) {
    cycletime = float(micros_current - micros_last) / 1000;
  }
  else { //micros() overflows approximately every 72 minutes.
    cycletime = float(0xFFFFFF - micros_last + micros_current) / 1000;
  }
  micros_last = micros_current;
}

void PT1(float in, float T, float &out) {
  float out_diff;
  float diff_T;

  if (firstrun) {
    out = in;
  } 
  else {
    out_diff = in - out;
    diff_T = abs(cycletime / T);
    if (diff_T < 1) {
      out = out + (out_diff * diff_T);
    }
    else {
      out = in;
    }
    if (abs(out) < 1.0e-7) {
      out = 0.0;
    }
  }
}

void measureAndCalculatePT1() {
  long pa_measurement;
  
  // Collect measurements
  do {
    pa_measurement = bmp.readPressure();
  } while (pa_measurement < 900);

  PT1(pa_measurement, 2000.0, pa_current_PT1);

  pa_current = pa_current_PT1;

  // Calculate minimum, maximum and center values
  if(pa_current > pa_max) {
    pa_max = pa_current;
    EEPROM.put(addr_long_hpa_max, pa_max);
  }
  
  if(pa_current < pa_min) {
    pa_min = pa_current;
    EEPROM.put(addr_long_hpa_min, pa_min);
  }
  
  calculateCenter();
}

#ifdef DEBUG
  void debugAddresses() {
    Serial.print(addr_int_addresses_start);
    Serial.print("\t");
    Serial.print(addr_long_hpa_min);
    Serial.print("\t");
    Serial.print(addr_long_hpa_max);
  }
  
  void debugButton() {
    switch_reset_eeprom = !digitalRead(PIN_RESET_EEPROM);
    Serial.print(switch_reset_eeprom);
  }

  void debugAcceleration() {
    acceleration_z = analogRead(PIN_ACCEL_Z);
    Serial.print(acceleration_z);
  }
  
  void debugLeds() {
    Serial.print(blue);
    Serial.print("\t");
    Serial.print(red);
    Serial.print("\t");
    Serial.print(0);
    Serial.print("\t");
    Serial.print(255);
  }
  
  void debugPressures() {
    Serial.print(pa_current);
    Serial.print("\t");
    Serial.print(pa_min);
    Serial.print("\t");
    Serial.print(pa_max);
    Serial.print("\t");
    Serial.print(pa_center);
  }
#endif
