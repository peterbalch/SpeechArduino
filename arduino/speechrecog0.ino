//-----------------------------------------------------------------------------
// Copyright 2021 Peter Balch
// ADC tester
//   reads ADC fast
//   sends value to PC
//-----------------------------------------------------------------------------

#include <Arduino.h>
#include <SPI.h>

//-----------------------------------------------------------------------------
// Defines, constants and Typedefs
//-----------------------------------------------------------------------------

// pins
const int AUDIO_IN = A7;

// get register bit - faster: doesn't turn it into 0/1
#ifndef getBit
#define getBit(sfr, bit) (_SFR_BYTE(sfr) & _BV(bit))
#endif

//-----------------------------------------------------------------------------
// Global Constants
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// testADCsimple
//-----------------------------------------------------------------------------
void testADCsimple(void)
{
  int i = analogRead(AUDIO_IN);
  Serial.println(i);
}

//-----------------------------------------------------------------------------
// testADCfast
//-----------------------------------------------------------------------------
void testADCfast(void)
{
  while (!getBit(ADCSRA, ADIF)) ; // wait for ADC
  int i = ADCL;
  i += ADCH << 8;
  bitSet(ADCSRA, ADIF); // clear the flag
  bitSet(ADCSRA, ADSC); // start ADC conversion

  Serial.println(i);
}

//-------------------------------------------------------------------------
// setup
//-------------------------------------------------------------------------
void setup(void)
{
  Serial.begin(57600);
  Serial.println("testADC");

  pinMode(AUDIO_IN, INPUT);
  analogReference(EXTERNAL);
  analogRead(AUDIO_IN); // initialise ADC to read audio input

  Serial.println("0 0 1024");
}

//-----------------------------------------------------------------------------
// Main routines
// loop
//-----------------------------------------------------------------------------
void loop(void)
{
//  testADCsimple();
  testADCfast();
}
