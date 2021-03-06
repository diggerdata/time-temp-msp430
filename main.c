#include <msp430.h>
#include "peripherals.h"
#include "stringstuff.h"
#include <stdint.h>
#include <stdlib.h>

typedef enum {B_START, B_CHANGE} state_b;

// Function Prototypes
void swDelay(char numLoops);
void adc_con(void);
void starttimerA2(void);

// Declare globals here

#define CALADC12_15V_30C  *((unsigned int *)0x1A1A)
#define CALADC12_15V_85C  *((unsigned int *)0x1A1C)

struct tm * global_time;
struct tm * last_global_time;
volatile unsigned int in_temp;
volatile unsigned int in_wheel;
volatile char temp_changed = 0;
unsigned long times[256];
float tempC[256];
unsigned int timercount;
unsigned int delay_cnt = 0;
char* time_str;
char* last_time_str;
char* date_str;
char* temp_c_str;
char* temp_f_str;
unsigned long utc;
volatile unsigned int temp_hist[4];
volatile unsigned int temp_index = 0;
volatile unsigned int temp_avg = 0;

#pragma vector=TIMER2_A0_VECTOR
__interrupt void TimerA2_ISR(void) {
    timercount++;
    delay_cnt++;
    if (timercount == 60000)
        timercount = 0;
    if (timercount % 100 == 0)
    {
        increment_tm(global_time, 1);
        adc_convert();
    }
}

#pragma vector=ADC12_VECTOR
__interrupt void ADC12_ISR(void) {
    in_temp = ADC12MEM0;
    in_wheel = ADC12MEM1;
    temp_hist[temp_index] = in_temp;
    temp_changed = 1;
    temp_index = (temp_index + 1) % 4;
}


// Main
void main(void) {
    volatile float temperatureDegC;
    volatile float temperatureDegF;
    volatile float degC_per_bit;
    volatile unsigned int bits30, bits85;
    WDTCTL = WDTPW | WDTHOLD;

    REFCTL0 &= ~REFMSTR;

    P8SEL &= ~BIT0;
    P8DIR |= BIT0;
    P8OUT |= BIT0;

    ADC12CTL0 = ADC12SHT0_9 | ADC12REFON | ADC12ON | ADC12MSC;
    ADC12CTL1 = ADC12SHP | ADC12CONSEQ_1;
    ADC12MCTL0 = ADC12SREF_1 + ADC12INCH_10;
    ADC12MCTL1 = ADC12SREF_0 + ADC12INCH_0 + ADC12EOS;

    __delay_cycles(100);
    ADC12CTL0 |= ADC12ENC;
    bits30 = CALADC12_15V_30C;
    bits85 = CALADC12_15V_85C;
    degC_per_bit = ((float)(85.0 - 30.0)) / ((float)(bits85 - bits30));

    ADC12IE = BIT1;
    _BIS_SR(GIE);

    global_time = (struct tm*)malloc(sizeof(struct tm));
    last_global_time = (struct tm*)malloc(sizeof(struct tm));
    initialize_tm(global_time);
    initialize_tm(last_global_time);
    configDisplay();
    configKeypad();
    starttimerA2();

    Graphics_clearDisplay(&g_sContext);

    state_b state = B_START;

    time_str = (char*)malloc(sizeof(char) * 9);
    last_time_str = (char*)malloc(sizeof(char) * 9);
    date_str = (char*)malloc(sizeof(char) * 7);
    temp_c_str = (char*)malloc(sizeof(char) * 8);
    temp_f_str = (char*)malloc(sizeof(char) * 8);
    convert_time(time_str, global_time);
    convert_time(last_time_str, global_time);
    convert_date(date_str, global_time);

    volatile int btn;

    while(1) {
        btn = getKey();

        switch (state) {
            case B_START:
                if (btn == '1') {
                    BuzzerOn();
                    state = B_CHANGE;
                }
                break;
            case B_CHANGE:
                if (btn == '2') {
                    BuzzerOff();
                    state = B_START;
                    break;
                }
                adc_con();
                long mapped = map(in_wheel, 0, 4095, 0, 256);
                BuzzerSetPwm(mapped);
                break;
            }

        if (temp_changed) {
            unsigned int i;
            temp_avg = 0;
            for (i=0; i<4; i++) {
              temp_avg += temp_hist[i];
            }
            temp_avg /= 4 ;

            temperatureDegC = (float) ((long) temp_avg - CALADC12_15V_30C )
                    * degC_per_bit + 30.0;
            temperatureDegF = temperatureDegC * 9.0 / 5.0 + 32.0;
            temp_changed = 0;
        }

        if (global_time->tm_min == 0) {
            BuzzerOn();
            swDelay(5000);
            BuzzerOff();
        }

        convert_temp(temp_c_str, temperatureDegC, 0);
        convert_temp(temp_f_str, temperatureDegF, 1);
        convert_time(time_str, global_time);
        convert_date(date_str, global_time);
        utc = mktime(global_time);

        times[utc % 256] = utc;
        tempC[utc % 256] = temperatureDegC;

        Graphics_drawStringCentered(&g_sContext, time_str, AUTO_STRING_LENGTH, 51, 10, OPAQUE_TEXT);
        Graphics_drawStringCentered(&g_sContext, date_str, AUTO_STRING_LENGTH, 51, 20, OPAQUE_TEXT);
        Graphics_drawStringCentered(&g_sContext, temp_c_str, AUTO_STRING_LENGTH, 51, 30, OPAQUE_TEXT);
        Graphics_drawStringCentered(&g_sContext, temp_f_str, AUTO_STRING_LENGTH, 51, 40, OPAQUE_TEXT);
        Graphics_flushBuffer(&g_sContext); // update display
    }
}


void adc_con() {
    ADC12CTL0 &= ~ADC12SC;
    ADC12CTL0 |= ADC12SC;
}

void starttimerA2(void) {
    TA2CTL = TASSEL_1 + MC_1 + ID_0;
    TA2CCR0 = 327;
    TA2CCTL0 = CCIE;
}

void swDelay(char numLoops) {
	volatile unsigned int i,j;	// volatile to prevent removal in optimization
			                    // by compiler. Functionally this is useless code

	for (j=0; j<numLoops; j++)
    {
    	i = 50000 ;					// SW Delay
   	    while (i > 0)				// could also have used while (i)
	       i--;
    }
}
