//-----------------------------------------------------------------------------
// Copyright 2021 Peter Balch
//   recognise words
//   subject to the GNU General Public License
//-----------------------------------------------------------------------------

#include <Arduino.h>
#include <SPI.h>
#include "Coeffs.h"
#include "Templates.h"

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

const byte SegmentSize = 50; //in mS
const byte hyster = 2;
const byte thresh = 100;

//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------

word CurBandData[nSegments][nBand + 1]; //current band data

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
  int n = 0;

  for (curSegment = 0; curSegment < nSegments; curSegment++) {
    zcr = 0;
    prevTime = 0;
    memset(valmax, 0, sizeof(valmax));

    prevTime = millis();
    while (millis() - prevTime < SegmentSize)
    {
      //    val = analogRead(AUDIO_IN);

      while (!getBit(ADCSRA, ADIF)) ; // wait for ADC
      val1 = ADCL;
      val2 = ADCH;
      bitSet(ADCSRA, ADIF); // clear the flag
      bitSet(ADCSRA, ADSC); // start ADC conversion
      val = val1;
      val += val2 << 8;
      //n++;

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
  for (seg = 0; seg < nSegments; seg++) {
    for (band = 0; band <= nBand; band++) {
      Serial.print(CurBandData[seg][band]);
      Serial.print(" ");
    }
    Serial.println("");
  }
}

//-----------------------------------------------------------------------------
// ShiftedDistance
//   distance from Utterance shifted by shift
//   to Templates[TemplateUtt]
//-----------------------------------------------------------------------------

int ShiftedDistance(int Utterance[nSegments][nBand+1], byte TemplateUtt, int8_t shift){
  byte band,seg,importance;
  int aUtterance[nSegments][nBand+1];
  int Dist,aMean,aSD;

  ShiftUtterance(Utterance, aUtterance, shift);

  Dist = 0;
  for (seg = 0; seg < nSegments; seg++) {
    if (seg == 0)
      importance = 2; else
      importance = 1;

    for (band = 0; band <= nBand; band++) {
      aMean = pgm_read_word(&Templates[TemplateUtt][seg][band].mean);
      aSD = pgm_read_word(&Templates[TemplateUtt][seg][band].sd);
      Dist += importance*abs(((long)aUtterance[seg][band])-aMean)*1000 / (50+aSD);
    }
  }

  return Dist;
}

//-----------------------------------------------------------------------------
// ShiftUtterance
//   shift an utterance by shift/SubShifts
//-----------------------------------------------------------------------------
void ShiftUtterance(int utSource[nSegments][nBand+1], int utDest[nSegments][nBand+1], int shift) {
  int8_t i,j,k,n;
  int m;
  byte seg,band;

  if (shift == 0) {
    for (seg = 0; seg < nSegments; seg++)
      for (band = 0; band <= nBand; band++)
        utDest[seg][band] = utSource[seg][band];
  } else {
    for (seg = 0; seg < nSegments; seg++)
    {
      n = SubShifts*(nSegments-1);
      i = shift*(nSegments-1);
      j = -i / (SubShifts*(nSegments-1));
      i = abs(i) % ((nSegments-1)*SubShifts);

      if (shift < 0)
        k = j+1; else
        k = j-1;
      for (band = 0; band <= nBand; band++) {
        if ((seg+j >= 0) && (seg+j < nSegments))
          m = utSource[seg+j][band]*((nSegments-1)*SubShifts-i) / n; else
          m = 0;
        if ((seg+k >= 0) && (seg+k < nSegments))
          m = m+utSource[seg+k][band]*i / n;
        utDest[seg][band] = m;
      }
    }
  }

  NormaliseUtterance(utDest);
}

//-----------------------------------------------------------------------------
// NormaliseUtterance
//-----------------------------------------------------------------------------
void NormaliseUtterance(int Utterance[nSegments][nBand+1]) {
  byte seg,band;
  long SegmentTotal,i;
  SegmentTotal = 0;
  for (seg = 0; seg < nSegments; seg++)
    for (band = 0; band <= nBand; band++)
      SegmentTotal += Utterance[seg][band];
  SegmentTotal = max(SegmentTotal,1);
  i = 50*nSegments*nBand;
  for (seg = 0; seg < nSegments; seg++)
    for (band = 0; band <= nBand; band++)
      Utterance[seg][band] = (i*Utterance[seg][band]) / SegmentTotal;
}

//-----------------------------------------------------------------------------
// FindBestShift
//-----------------------------------------------------------------------------
int FindBestShift(int Utterance[nSegments][nBand+1], int TemplateUtt) {
  int dist,BestShift,BestShiftDist,shift;
  BestShiftDist = 0x7FFF;
  for (shift = -MaxShift; shift <= MaxShift; shift++) {
    dist = ShiftedDistance(Utterance,TemplateUtt,shift);
    if (dist < BestShiftDist) {
      BestShiftDist = dist;
      BestShift = shift;
    }
  }
  return BestShift;
}

//-----------------------------------------------------------------------------
// FindBestUtterance
//   which template best fits the utterance 
//-----------------------------------------------------------------------------
int FindBestUtterance(int Utterance[nSegments][nBand+1], int *BestDist) {
  int dist,BestUttDist,shiftDist;
  int BestUtt,shift,TemplateUtt;

  BestUtt = 0;
  *BestDist = 0;
  BestUttDist = 0x7FFF;
  for (TemplateUtt = 0; TemplateUtt < nUtterances; TemplateUtt++) {
    shift = FindBestShift(Utterance,TemplateUtt);
    shiftDist = ShiftedDistance(Utterance,TemplateUtt,shift);

    if (shiftDist < BestUttDist) {
      BestUttDist = shiftDist;
      BestUtt = TemplateUtt;
      *BestDist = BestUttDist;
    }
  }
  return BestUtt;
}

//-----------------------------------------------------------------------------
// SendUtterance
//-----------------------------------------------------------------------------
void SendUtterance(int Utterance[nSegments][nBand+1])
{
  byte seg,band;
  for (seg = 0; seg < nSegments; seg++) {
    for (band = 0; band <= nBand; band++) {
      Serial.print(Utterance[seg][band]);
      Serial.print(" ");
    }
    Serial.println("");
  }
}

//-----------------------------------------------------------------------------
// CheckSerial
//-----------------------------------------------------------------------------
void CheckSerial(void)
{
  byte seg,band,i,j;
  static int Utterance[nSegments][nBand+1];
  int Utterance2[nSegments][nBand+1];
  int dist;
  char buffer[30];

  if ( Serial.available() > 0 ) {
    switch (GetSerial()) {
      case 'u': 
        for (seg = 0; seg < nSegments; seg++) {
          for (band = 0; band <= nBand; band++) {
            Utterance[seg][band] = GetSerial();
            Utterance[seg][band] += 256*GetSerial();
          }
        }
        Serial.println('u');
        SendUtterance(Utterance);          
        break;
      case 's': 
        ShiftUtterance(Utterance,Utterance2,(int8_t)GetSerial());
        Serial.println('s');
        SendUtterance(Utterance2);          
        break;
      case 'd': 
        Serial.println('d');
        i = (int8_t)GetSerial();
        j = (int8_t)GetSerial();
        Serial.println(ShiftedDistance(Utterance, i, j));
        break;
      case 'b': 
        Serial.println('b');
        Serial.println(FindBestShift(Utterance, GetSerial()));
        break;
      case 'f': 
        Serial.println('f');
        i = FindBestUtterance(Utterance,&dist);
        strcpy_P(buffer, (char *)pgm_read_word(&(sUtterances[i])));
        Serial.print(buffer);
        Serial.print(" ");
        Serial.println(i);
        break;
    }    
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
  CheckSerial();

  PollBands(false);
}
