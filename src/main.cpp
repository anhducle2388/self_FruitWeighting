#include <Arduino.h>

/////////////////////////////////////////
/* PRE-DEFINED FUNCTIONS AND VARIABLE S*/
#define BAUDRATE  115200 // Hz
#define TSERIAL   500    // ms
#define TSAMPLING 100    // ms: Max 65000*2 ms -> If need longer sampling time, need to config Timer Register at configureRoutineInteruptT()

void configureGpio(void);
void configureClk(void);
void configureRoutineInteruptT(void);
void configureAdcLoadcell(void);
void configureHx711(void);

void measAdcLoadcell(void);
uint32_t measAdcHx711(void);

float getValHx711(uint32_t adcVal, uint16_t scale);
float getValLoadcell(uint16_t adcVal, uint16_t scale);

volatile uint16_t curCnt = 0;
volatile uint32_t valDLoadcell = 0;
volatile float    valFLoadcell = 0.0f;

//////////////////////////////////
/* SETUP HARDWARE CONFIGURATION */
void setup() {
  
  // Init Serial Port with Baudrate
  Serial.begin(BAUDRATE);
  Serial.println("");
  Serial.println(">>> Initializing");
  delay(1000);

  // Config Function
  configureGpio();
  configureAdcLoadcell();
  configureHx711();
  configureRoutineInteruptT();
  
  // Finish Setup Configuration
  Serial.println(">>> Start Program");
  delay(1000);

  // Global interupt Enable + Low Power Mode 0
  _BIS_SR(GIE);
}

void configureGpio(void) {
  // Config Output LED at P1.0 Red Led
  P1DIR |= BIT0;    /*Config in/out in Direction bit = 1*/
  P1OUT |= BIT0;    /*Output is High*/

  // Config Output LED at P4.7 Green Led
  P4DIR |= BIT7;    /*Config in/out in Direction bit = 1*/
  P4OUT |= BIT7;    /*Output is High*/

  // Config Input Button at Px.1
  P1DIR &= ~BIT1;   /*Config in/out in Direction bit = 0*/
  P1REN |= BIT1;    /*Enable Register Enable for Pull-Up/Down Register*/
  P1OUT |= BIT1;    /*Config Pull-Up for DI Mode*/
  P1IE  |= BIT1;    /*Config Interupt for P2.1 Button*/
  P1IES |= BIT1;    /*Config Interupt for Falling Edge*/
  P1IFG &= ~BIT1;   /*Clear Interupt Flag at Px.1*/
}

void configureRoutineInteruptT(void) {
  // TA0CTL: Timer-A0 Control Register
  //  TASSEL_2: Source from SMCLK
  //  MC_1:     CountUp to TA0CCR0
  //  TAIE:     Enable Interupt
  TA0CTL |= TASSEL_2 + ID_0 + MC_3 + TAIE;
  TA0CCR0 = (uint16_t) TSAMPLING / 2 * 1000 - 1;
}

void configureAdcLoadcell(void) {
  // Config Pin Analog Input
  P6SEL |= BIT6; // Select periph function on SEL register

  // Config ADC Function
  ADC12CTL0 = ADC12SHT0_2 | ADC12ON;       // Select ADC12 sample and hold time opt = 3 and turn on ADC12
  ADC12CTL1 = ADC12SHP;                    // Select ADC12 sample-and-hold pulse-mode detection.
  ADC12CTL2  |= ADC12RES_2;                // Select ADC12 bit conversion.
  ADC12IE    |= ADC12IE0;                  // Enable interupt on MEM0
  ADC12MCTL0 |= ADC12INCH_6 | ADC12SREF_3; // Select A6 P6.6 and Vref Opt 3 ????????????????????????
}

void configureHx711(void) {
  // P4.1 <-> CLK/OUT
  P4DIR |= BIT1;    /*Config out in Direction bit = 1*/
  P4OUT &= ~BIT1;   /*Output is Low in default*/

  // P4.2 <-> DAT/IN 
  P4DIR &= ~BIT2;   /*Config in Direction bit = 0*/
  P4REN |= BIT2;    /*Enable Register Enable for Pull-Up/Down Register*/
  P4OUT |= BIT2;    /*Config Pull-Up for DI Mode*/
}

void measAdcLoadcell(void) { 
  ADC12CTL0 |= ADC12ENC | ADC12SC;  // Enable and start ADC conversion
}

uint32_t measAdcHx711(void) {
  uint8_t  cntAdc = 0;
  uint32_t adcVal = 0;

  // Retrieve 24b DATA from Hx711 
  do {
    // CLK trigger
    P4OUT |= BIT1;
    adcVal = adcVal << 1;
    P4OUT &= ~BIT1;
    if (digitalRead(P4_2)) {
      adcVal++;
    }
    cntAdc++;
  } while (cntAdc < 24);

  P4OUT |= BIT1;
  adcVal = adcVal ^ 0x800000;
  P4OUT &= ~BIT1;

  return adcVal;
}

float getValLoadcell(uint16_t adcVal, uint16_t scale = 1000) {
  return (float) adcVal / 4095 * scale;
}

float getValHx711(uint32_t adcVal, uint16_t scale = 1000) {
  return (float) adcVal / 16777215 * scale;
}


////////////////////////////////////
/* MAIN PROGRAM BLOCK - MAIN LOOP */
void loop() {

  /*Sending debug log every seconds*/
  Serial.println(valFLoadcell);
  delay(TSERIAL);

}

////////////////////////////
/* INTERUPT PROGRAM BLOCK */
#pragma vector=PORT1_VECTOR
__interrupt void toggleRedLed(void) {
  P1IFG &= ~BIT1;   

  // Turn off = 1 at P1.0 RED LED
  P1OUT ^= BIT0;
}

#pragma vector=ADC12_VECTOR
__interrupt void adc12bLoadcell(void) {
  ADC12IFG &= ~ADC12IFG0;

  // Get ADC result at Mem0 with 12b-Masked
  valDLoadcell = (uint16_t) ADC12MEM0 & 0x0FFF;
}

#pragma vector=TIMER0_A1_VECTOR
__interrupt void timerRoutine(void) {
  TA0CTL &= ~TAIFG;

  // Toggle GREEN LIGHT to indicate operating
  P4OUT ^= BIT7; curCnt++;

  // Main Routine Program
  valDLoadcell = measAdcHx711();
  valFLoadcell = getValHx711(valDLoadcell, 100);
}