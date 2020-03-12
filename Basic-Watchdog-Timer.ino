// ATtiny85 Watchdog Timer
// Author: Chip McClelland
// Date: 12-11-19
// License - GPL3

// ATMEL ATTINY 25/45/85 / ARDUINO
//
//                           +-\/-+
//  Reset - Ain0 (D 5) PB5  1|    |8  Vcc
//  Wake  - Ain3 (D 3) PB3  2|    |7  PB2 (D 2) Ain1 - SCL
//  Done  - Ain2 (D 4) PB4  3|    |6  PB1 (D 1) MISO - Reset uC
//                     GND  4|    |5  PB0 (D 0) MOSI - SDA
//                           +----+
// Interrupt code: https://gammon.com.au/forum/?id=11488&reply=9#reply9

/*
This is my dream Watchdog timer. It will function exactly as I want it - unlike the TPL5010.
First - You can set the interval in the header - wakeIntervalSeconds
Second - At the end of the interval, it will bring WAKE HIGH
Third - It will start a timer to reset which you can set in the header - resetIntervalSeconds
Finally - It will either Reset the device or restart the interval if it received a HIGH / LOW on Debounce
Version 1.0 - Minimally Viable Product - No Sleep
*/

#define I2C_SLAVE_ADDRESS 0x4                 // i2c slave address (04)
#define adc_disable() (ADCSRA &= ~(1<<ADEN))  // disable ADC (before power-off)


#include <avr/power.h>    // Power management
#include <TinyWireS.h>    // Tiny Wire Slave Library 

enum State {INITIALIZATION_STATE, IDLE_STATE, INTERRUPT_STATE, DONE_WAIT_STATE, RESET_STATE};
State state = INITIALIZATION_STATE;

// Pin assignments will for the ATTINY85
const int sdaPin    =   0;              // Pin we use to communicate data via i2c
const int resetPin  =   1;              // Pin to reset the uC - Active LOW
const int sclPin    =   2;              // Pin we use to communicate via i2c clock 
const int wakePin   =   3;              // Pin that wakes the uC - Active HIGH
const int donePin   =   4;              // Pin the uC uses to "pet" the uC
 

// Timing Variables
const unsigned long wakeIntervalSeconds = 3660UL;     // One Hour and one minute
const int resetIntervalSeconds = 5;     // You have one minute to pet the watchdog
unsigned long lastWake = 0;
unsigned long resetWait = 0;

// Program Variables
volatile bool donePinInterrupt = false;

void setup() {
  pinMode(resetPin,OUTPUT);             // Pin to reset the uC
  pinMode(wakePin,OUTPUT);              // Pin to wake the uC
  pinMode(donePin,INPUT);               // uC to Watchdog pin

  TinyWireS.begin(I2C_SLAVE_ADDRESS);      // init I2C Slave mode

  digitalWrite(resetPin, HIGH);
  digitalWrite(wakePin, HIGH);
  delay(10000);

  digitalWrite(resetPin, HIGH);
  digitalWrite(wakePin, LOW);

  adc_disable();

  PCMSK  |= bit (PCINT4);               // Pinchange interrupt on pin D4 / pin 3
  GIFR   |= bit (PCIF);                 // clear any outstanding interrupts
  GIMSK  |= bit (PCIE);                 // enable pin change interrupts

  state = IDLE_STATE;
}

void loop() {
  switch (state) {
    case IDLE_STATE:
      if (millis() - lastWake >= wakeIntervalSeconds * 1000UL) {
        state = INTERRUPT_STATE;
      }
      if (donePinInterrupt) {           // This is where we can reset the wake cycle using the Done pin
        lastWake = millis();
        donePinInterrupt = false;
      }
    break;

    case INTERRUPT_STATE:              // In this state, we will take the average of the non-event pressures and compute an average
      digitalWrite(wakePin, HIGH);
      resetWait = millis();
      state = DONE_WAIT_STATE;
    break;
    
    case DONE_WAIT_STATE:
      if (millis() - resetWait >= resetIntervalSeconds *1000) {
        digitalWrite(wakePin,LOW);
        state = RESET_STATE;
      }
      else if (donePinInterrupt) {
        donePinInterrupt = false;
        digitalWrite(wakePin,LOW);
        lastWake = millis();
        state = IDLE_STATE;
      }

    break;

    case RESET_STATE:
      digitalWrite(resetPin, LOW);      // Reset is active low
      delay(5000);                       // Need to see how long this should be
      digitalWrite(resetPin, HIGH);     // Need to bring high for device to reset
      lastWake = millis();
      state = IDLE_STATE;
    break;
  }
}

ISR (PCINT0_vect) {
  donePinInterrupt = true;
}

void tws_delay(uint16_t mSec) {
  unsigned long start = millis();
  while (millis() - start < mSec) {
    TinyWireS_stop_check();
  }
}
