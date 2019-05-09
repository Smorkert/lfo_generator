#include "lfo_utility.h"

//defining the output PWM parameters
#define PWM_FREQ 0x00FF // PWM frequency - 31.25KHz
#define PWM_MODE 0 // Fast (1) or Phase Correct (0)
#define PWM_QTY 2 // 2 PWMs in parallel
 
//LFO V=varibales
uint16_t lfo, unPhase, untPhase, RATE = 0, SHAPE = 0; //changed RATE, SHAPE from volatile uint16_t if issues
uint16_t y = 0;
volatile uint16_t p_rate, rate, r_timer;
uint32_t phase, phase_inc, tap_phase_inc;
volatile uint8_t waveform, ovf; 

//tap tempo variables
uint8_t tap_state, new_tap = 0, n = 0, tap_phase = 0, c_tap = 1, p_tap = 1, tap_flag;
volatile uint32_t timer, tap_rate_sum, tap_rate, timer_buffer[64];
uint8_t b_timer;

// PROGMEM stores the values in the program memory
// create lfo frequency lookup table (512, 0-511)
PROGMEM  const uint32_t lfo_table[]  = {
  #include "lfo_table.h"
};
//sine wave table
PROGMEM  const uint16_t sine_table[]  = {
  #include "sinewave_table.h"
};
//triangle wave table
PROGMEM  const uint16_t tri_table[]  = {
  #include "triangle_table.h"
};
//saw wave table down
PROGMEM  const uint16_t saw_table[]  = {
  #include "saw_table.h"
};
/*
//saw wave table up
PROGMEM  const uint16_t saw_table2[]  = {
  #include "saw_table2.h"
};
*/


void setup() {
  //setup PWM
  TCCR1A = (((PWM_QTY - 1) << 5) | 0x80 | (PWM_MODE << 1));
  TCCR1B = ((PWM_MODE << 3) | 0x11); // ck/1
  TIMSK1 = 0x20; // interrupt on capture interrupt
  ICR1H = (PWM_FREQ >> 8);
  ICR1L = (PWM_FREQ & 0xff);
  DDRB |= ((PWM_QTY << 1) | 0x02); // turn on outputs

  //setup ADCs
  ADMUX = 0x40; // start with ADC0 - internal VCC for Vref
  ADCSRA = 0xe7; // ADC enable, autotrigger, ck/128
  ADCSRB = 0x00; // free running mode
  DIDR0 = ((1 << ADC0D)|(1 << ADC1D)); //disable digital IO for ADC0 and ADC1

  //Setup Button
  DDRD &= ~(0x04); //set PD2 to input
  PORTD |= (0x04); // turn on the Pull-up 
  
  sei(); // turn on interrupts - not really necessary with arduino
}
 
void loop(){
  while(1); 
}

//Timer 1 ISR
ISR(TIMER1_CAPT_vect){

  //Read ADC pins
  readADC(&RATE, &SHAPE);
  rate = (RATE>>7); //scale
  //set waveshape
  waveform = (uint8_t)(SHAPE>>13);  //scale to 3-bit value,
  //switch back to reading rate knob if changed
  if(rate != p_rate){
    tap_phase = 0;
  }
  p_rate = rate; //store previous rate 

  //determine phase step between tap tempo or pot
  switch(tap_phase){
    case 0:
      //get phase increment from lookup table
      phase_inc = pgm_read_dword_near(lfo_table + rate); //scale to 0-511
      //accumulate phase 
      phase += phase_inc;
      //overflow flag
      ovf = (phase>>30);
      //check overflow
      phase &= 0x3FFFFFFF;
      //get lfo value, scale down to 0-511
      unPhase = (uint16_t)(phase>>21);
    break;
    case 1:
      //phase increment from tap tempo
      untPhase += 4096;
      if(untPhase > tap_phase_inc){
        //increment unPhase
        unPhase++;
        //overflow flag
        ovf = (unPhase>>9);
        //check overflow
        unPhase &= 0x01FF;
        //clear untPhase
        untPhase -= tap_phase_inc;
      }
    break;
  }

  // waveform select
  switch(waveform){
    //sine
    case 0:
      lfo = pgm_read_word_near(sine_table + unPhase);
    break;
    //triganle
    case 1:
      lfo = pgm_read_word_near(tri_table + unPhase);
    break;
    //saw down
    case 2:
      lfo = pgm_read_word_near(saw_table + unPhase);
    break;
    //saw up
    case 3:
      unPhase = 511-unPhase;
      lfo = pgm_read_word_near(saw_table + unPhase);
    break;
    //square
    case 4:
      lfo = (uint16_t)((unPhase>>8)-1);
    break;
    //pulse (quarter)
    case 5:
      lfo = (uint16_t)((unPhase>>7)-1);
    break;
    //step
    case 6:
      switch(unPhase>>6){
        case 0:
          lfo = 0;
        break;
        case 1:
          lfo = 16384;
        break;
        case 2:
          lfo = 32768;
        break;
        case 3:
          lfo = 49152;
        break;
        case 4:
          lfo = 65535;
        break;
        case 5:
          lfo = 49152;
        break;
        case 6:
          lfo = 32768;
        break;
        case 7:
          lfo = 16384;
        break;
      }
    break;
    //random
    case 7:
      if(ovf == 1){
        //generate new random value at user defined rate 
        lfo += (r_timer && 0x1FFF); // seeded with a different number
        lfo ^= lfo << 2;
        lfo ^= lfo >> 7;
        lfo ^= lfo << 7;
        r_timer++; //increment random see value
      }
    break;
  }
  
  //write the PWM output signal
  OCR1AL = (uint8_t)(lfo >> 8); // send out high byte
  OCR1BL = (uint8_t)(lfo & 0xFF); //send out low byte
  

  //BUTTON STATUS BLOCK
  //increment button timer
  b_timer++;
  //count to 8 (3906.25KHz)
  if(b_timer >= 8){
    //read tap tempo button
    c_tap = (PIND & 0x04)>>2;
    //determine change
    if(c_tap != p_tap){
      //check for falling edge
      if(c_tap == 0){
        //button pressed, set tap flag
        tap_flag = 1;
      }
    }
    p_tap = c_tap; //store previous tap
  }
  //check overflow
  b_timer &= 0x07;


  //STATE DETECTION BLOCK
  if(tap_flag == 1){
    if(new_tap == 0){
      //new tap detected
      tap_state = 1; //conting state
      timer = 0; //reset timer, necessary?
      new_tap = 1; //prepare to store value
    }
    else{
      tap_state = 2; //store value and reset
    }
    tap_flag = 0; //clear tap flag
  }


  //TEMPO DETERMINTATION BLOCK
  switch(tap_state){
    //do nothing
    case 0:
    break;  
    
    //count
    case 1:
      //increment timer
      timer++;
      //check for timeout, 4 seconds
      if(timer > 0x0001E848){
        new_tap = 0;
        //clear timer average buffer
        for(uint8_t i = 0; i<n; i++){
          timer_buffer[i] = 0;
        }
        tap_rate_sum = 0; //clear tap rate
        n = 0;  //clear quantity
        timer = 0; //reset timer
        tap_state = 0;
      }
    break;
  
    //store
    case 2:
      timer_buffer[n] = timer; //store value
      tap_rate_sum += timer_buffer[n]; //sum for average 
      n++; //increment average count
      tap_phase_inc = ((tap_rate_sum/n)<<12)>>9; //calculate period and increase resolution (12-lshift: 4096)
      if(n > 1) {
        tap_phase = 1; //use tap tempo value for rate / phase increments
      }
      timer = 0; //reset timer
      tap_state = 1; //return to counting state 
    break;
  }
}
