//-----------------------------------------------------------------------------
// Copyright 2021 Peter Balch
//   digital filter tester
//   subject to the GNU General Public License
//-----------------------------------------------------------------------------

#include <Arduino.h>
#include <SPI.h>
#include "Coeffs.h"

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

const byte NumSegments = 13;
const byte SegmentSize = 50; //in mS
const byte hyster = 2;
const byte thresh = 100;

//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------

word CurBandData[NumSegments][nBand + 1]; //current band data

//-------------------------------------------------------------------------
// GetSerial
//-------------------------------------------------------------------------
byte GetSerial() {
  while ( Serial.available() == 0 ) ;
  return Serial.read();
}

//-------------------------------------------------------------------------
// PollBands
//-------------------------------------------------------------------------
bool PollBands(bool init)
{
  bool IsPos, Collecting;
  static unsigned long prevTime;
  byte band, seg, val1, val2;
  const byte hyster = 20;
  static int zero = 500;
  static byte curSegment = 255;
  long val;
  word zcr;
  static int valmax[10];
  static int pd[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  static int ppd[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  static int ppval = 0;
  static int pval = 0;

  if (init)
  {
    memset(pd, 0, sizeof(pd));
    memset(ppd, 0, sizeof(ppd));
    memset(CurBandData, 0, sizeof(CurBandData));
    pval =  0;
    ppval =  0;
    return false;
  }

  val = 0;
  IsPos = true;
  Collecting = false;

  for (curSegment = 0; curSegment < NumSegments; curSegment++) {
    zcr = 0;
    prevTime = 0;
    memset(valmax, 0, sizeof(valmax));

    prevTime = millis();
    while (millis() - prevTime < SegmentSize)
    {
      while (!getBit(ADCSRA, ADIF)) ; // wait for ADC
      val1 = ADCL;
      val2 = ADCH;
      bitSet(ADCSRA, ADIF); // clear the flag
      bitSet(ADCSRA, ADSC); // start ADC conversion
      val = val1;
      val += val2 << 8;

      if (val < zero)
        zero--; else
        zero++;
      val = val - zero;

      if (!Collecting) {
        if (abs(val) > thresh)
          Collecting = true; else
          return false;
      }

      if (IsPos)
      {
        if (val < -hyster)
        {
          IsPos = false;
          zcr++;
        }
      } else {
        if (val > +hyster)
        {
          IsPos = true;
          zcr++;
        }
      }
      ppval = pval;
      pval = val;

      for (band = 0; band < nBand; band++)
      {
        int L1, L2;
        L1 =  ((-(filt_b1[band]) * pd[band] - filt_b2[band] * ppd[band]) >> 16) + val;
        L2 = (filt_a0[band] * L1 - filt_a0[band] * ppd[band]) >> 16;
        ppd[band] = pd[band];
        pd[band] = L1;
        if (abs(L2) > valmax[band])
          valmax[band]++;
      }
    }

    for (band = 0; band < nBand; band++)
      CurBandData[curSegment][band + 1] = valmax[band];
    CurBandData[curSegment][0] = zcr;
  }

  if (Collecting) {  
    PrintCurBandData();
    delay(500);
  }

  return Collecting;
}

//-----------------------------------------------------------------------------
// PrintCurBandData
//-----------------------------------------------------------------------------
void PrintCurBandData(void)
{
  byte seg, i, band;

  Serial.println("a");
  for (seg = 0; seg < NumSegments; seg++) {
    for (band = 0; band <= nBand; band++) {
      Serial.print(CurBandData[seg][band]);
      Serial.print(" ");
    }
    Serial.println("");
  }
}

//-------------------------------------------------------------------------
// setup
//-------------------------------------------------------------------------
void setup(void)
{
  Serial.begin(57600);
  Serial.println("speechrecog");

  pinMode(AUDIO_IN, INPUT);
  analogReference(EXTERNAL);
  analogRead(AUDIO_IN); // initialise ADC to read audio input

  Serial.println("0 0 0 0 0 0 350");

  PollBands(true);
}

//-----------------------------------------------------------------------------
// Main routines
// loop
//-----------------------------------------------------------------------------
void loop(void)
{
  PollBands(false);
}
