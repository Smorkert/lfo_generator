#define SAMPLERATE 31250.00
#define LFO_FREQ 512
#define HYST 32

uint32_t SAMPLES_PER_CYCLE_FIXEDPOINT;
uint16_t SAMPLES_PER_CYCLE = 512;
float TICKS_PER_CYCLE;
uint32_t nLFOphase[LFO_FREQ];
float Frequencies[LFO_FREQ];

uint8_t _i = 130;
uint16_t _mod0temp = 0x0000;
uint16_t _mod1temp = 0x0000;

//Generate frequencies and time increments with fixed point math
//Fill the frequency table with the phase increment values we require to generate the lfo frequencies
//This is executed outside the program, only here for reference 
void createNoteTable(float fSampleRate)
{
    SAMPLES_PER_CYCLE_FIXEDPOINT  = (uint32_t)SAMPLES_PER_CYCLE<<21;
    TICKS_PER_CYCLE = (float)((float)SAMPLES_PER_CYCLE_FIXEDPOINT/(float)SAMPLERATE);
    for(uint32_t unFreq = 0; unFreq < LFO_FREQ; unFreq++)
    {
        // Correct calculation for frequency
        float fFrequency = ((pow(2.0,(unFreq-256.0)/51.0)) * 2);
        Frequencies[unFreq] = fFrequency;
        nLFOphase[unFreq] = fFrequency*TICKS_PER_CYCLE;
    }
}

//This function updates mod0vlaue and mod1value. 10-bit value in uint16_t format
static inline void readADC(uint16_t* _mod0value, uint16_t* _mod1value) {
    if (ADCSRA & (1 << ADIF)) { // check if sample ready
      --_i; // check which sample we are on
      if (_i == 129) { // do nothing, first sample after mux change
      }
      else if (_i >= 65) { // sample ADC0
        _mod0temp += ADCL; // fetch ADCL first to freeze sample
        _mod0temp += (ADCH << 8); // add to temp register
        if (_i == 65) { // check if enough samples have been averaged
          // add in hysteresis to remove jitter
          if (((_mod0temp - *_mod0value) < HYST) || ((*_mod0value - _mod0temp) < HYST)) {
          }
          else {
            *_mod0value = _mod0temp; // move temp value
      }
          _mod0temp = 0x0000; // reset temp value
          ADMUX = 0x41; // switch to ADC1
        }
      }
      else if (_i < 64) { // sample ADC1, first sample (64) after mux change ignored
        _mod1temp += ADCL; // fetch ADCL first to freeze sample
        _mod1temp += (ADCH << 8); // add to temp register
        if (_i == 0) { // check if enough samples have been averaged
          // add in hysteresis to remove jitter
          if (((_mod1temp - *_mod1value) < HYST) || ((*_mod1value - _mod1temp) < HYST)) {
          }
          else {
            *_mod1value = _mod1temp; // move temp value
      }
          _mod1temp = 0x0000; // reset temp value
          ADMUX = 0x40; // switch to ADC0
          _i = 130; // reset counter
        }
      }
      ADCSRA = 0xf7; // reset the interrupt flag
    }
}

