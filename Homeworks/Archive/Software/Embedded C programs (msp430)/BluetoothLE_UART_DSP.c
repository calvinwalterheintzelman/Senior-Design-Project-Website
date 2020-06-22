#include <msp430.h>
#include <stdio.h>
#include <stdbool.h>
#include "include/bass.h"
#include "include/mid.h"
#include "include/high.h"
#include "include/all_pass.h"
#include "include/DSPLib.h"
//#include "DSPLib.h"

#define RECV_STREAM_LENGTH 19
#define ADC_BUFF_LEN 100

unsigned char recvBufferTemp[RECV_STREAM_LENGTH] = {0};
unsigned int recvBufferTemp_Index = 0;
int index = 0;

int ADC_index = 0;

/* Filter input signal */
DSPLIB_DATA(adc,4)
_q15 adc[ADC_BUFF_LEN] = {0}; // array of ADC transfered values

bool hasPendingUpdate = false;

int equalization1_value = 50;
int equalization2_value = 50;
int equalization3_value = 50;
int distortion_value = 0;
int reverb_value = 0;

/* Hard Clipping Value */
//HardClip value is (100 - reverb_value)/100 * MAXINT
int hardclip_value = INT16_MAX;

/* Bass Filter result */
DSPLIB_DATA(bass_result,4)
_q15 bass_result[ADC_BUFF_LEN];

/* Bass Filter Parameters */
#define FILTER_LENGTH (ADC_BUFF_LEN)
#define BASS_FILTER_STAGES (sizeof(FILTER_COEFFS_BASS)/sizeof(msp_biquad_df1_q15_coeffs))

/* Bass Filter coefficients */
DSPLIB_DATA(bass_filterCoeffs,4)
msp_biquad_df1_q15_coeffs bass_filterCoeffs[BASS_FILTER_STAGES];

/* Bass State Buffer */
DSPLIB_DATA(bass_states,4)
msp_biquad_df1_q15_states bass_states[BASS_FILTER_STAGES];

/* Initialize scaling factor for BASS */
msp_scale_q15_params bass_scale;

/* MID Filter result */
DSPLIB_DATA(mid_result,4)
_q15 mid_result[ADC_BUFF_LEN];

/* MID Filter Parameters */
#define FILTER_LENGTH (ADC_BUFF_LEN)
#define MID_FILTER_STAGES (sizeof(FILTER_COEFFS_MID)/sizeof(msp_biquad_df1_q15_coeffs))

/* MID Filter coefficients */
DSPLIB_DATA(mid_filterCoeffs,4)
msp_biquad_df1_q15_coeffs mid_filterCoeffs[MID_FILTER_STAGES];

/* MID State Buffer */
DSPLIB_DATA(mid_states,4)
msp_biquad_df1_q15_states mid_states[MID_FILTER_STAGES];

/* Initialize scaling factor for MID */
msp_scale_q15_params mid_scale;


/* HIGH Filter result */
DSPLIB_DATA(high_result,4)
_q15 high_result[ADC_BUFF_LEN];

/* HIGH Filter Parameters */
#define FILTER_LENGTH (ADC_BUFF_LEN)
#define HIGH_FILTER_STAGES (sizeof(FILTER_COEFFS_HIGH)/sizeof(msp_biquad_df1_q15_coeffs))

/* HIGH Filter coefficients */
DSPLIB_DATA(high_filterCoeffs,4)
msp_biquad_df1_q15_coeffs high_filterCoeffs[HIGH_FILTER_STAGES];

/* HIGH State Buffer */
DSPLIB_DATA(high_states,4)
msp_biquad_df1_q15_states high_states[HIGH_FILTER_STAGES];

/* Initialize scaling factor for HIGH */
msp_scale_q15_params high_scale;

/* Initialize scaling factor for REVERB */
msp_scale_q15_params reverb_scale;

/* Equalization Result */
DSPLIB_DATA(eq_result,4)
_q15 eq_result[ADC_BUFF_LEN];

/* Previous Output */
DSPLIB_DATA(prev_result,4)
_q15 prev_result[ADC_BUFF_LEN];

void send_data (unsigned char data){
    while(!(UCA3IFG & UCTXIFG));  //wait until tx buffer is empty before loading new data
    UCA3TXBUF = data;
}

int hexToInt(char c){ //convert single char into int
    if('0'<=c && c<='9'){ //chars '0'-'9'
        return (int) c - '0';
    }
    if('A'<=c && c<='F'){ //chars 'A' - 'F'
        return (int) c - 'A';
    }
    return 0;
}

void update_slider_value(){
/* Function: update_slider_value
 * This function is called when 'hasPendingUpdate' is set to true
 * Scans the recvBufferTemp for the update message and updates the corresponding value accordingly
 *
 */
    unsigned int msg_index;
    for(msg_index = 0; msg_index < RECV_STREAM_LENGTH; msg_index++){ //for every character in the recv stream
            if(recvBufferTemp[msg_index] == 'N'){ //if this character is 'N'
                //index of ID is message start index + 9 and 10
                //index of message value is message start index + 12 and 13

                //FIRST: extract the slider ID
                unsigned char msg_ID[2] = {0}; //init the message ID
                msg_index += 9; //add 9 to reach the first value of ID in message
                if(msg_index > (RECV_STREAM_LENGTH-1)){ //adjust for looping message around buffer
                    msg_index = msg_index - RECV_STREAM_LENGTH;
                }
                msg_ID[0] = recvBufferTemp[msg_index]; //save the first ID char
                msg_index += 1; //add 1 to reach the second value of ID in message
                if(msg_index > (RECV_STREAM_LENGTH-1)){ //adjust for looping message around buffer
                    msg_index = msg_index - RECV_STREAM_LENGTH;
                }
                msg_ID[1] = recvBufferTemp[msg_index]; //save the second ID char

                //SECOND: extract the slider value
                unsigned int hexValue = 0; //value of slider
                msg_index += 2; //add 2 to reach the first hex value of slider in message
                if(msg_index > (RECV_STREAM_LENGTH-1)){ //adjust for looping message around buffer
                    msg_index = msg_index - RECV_STREAM_LENGTH;
                }
                hexValue = hexToInt(recvBufferTemp[msg_index]) << 4; //convert first hex value to corresponding int
                msg_index += 1; //add 1 to reach the second hex value of slider in message
                if(msg_index > (RECV_STREAM_LENGTH-1)){ //adjust for looping message around buffer
                    msg_index = msg_index - RECV_STREAM_LENGTH;
                }
                hexValue |= hexToInt(recvBufferTemp[msg_index]); //convert second hex value to corresponding int
                //All identifiers have a unique 2nd character, might not need to look at first character at all
                //Equalization 1 ID: 002A
                //Equalization 2 ID: 002C
                //Equalization 3 ID: 002E
                //Distortion ID:     0030
                //Reverb ID:         0032
                if(msg_ID[1] == 'A'){
                    equalization1_value = hexValue;
                    bass_scale.scale = INT16_MAX/100 * equalization1_value;
                }else if(msg_ID[1] == 'C'){
                    equalization2_value = hexValue;
                    mid_scale.scale = INT16_MAX/100 * equalization2_value;
                }else if(msg_ID[1] == 'E'){
                    equalization3_value = hexValue;
                    high_scale.scale = INT16_MAX/100 * equalization3_value;
                }else if(msg_ID[1] == '0'){
                    distortion_value = hexValue;
                    hardclip_value = (100 - distortion_value) * INT16_MAX / 100;
                }else if(msg_ID[1] == '2'){
                    reverb_value = hexValue;
                    reverb_scale.scale = INT16_MAX/200 * reverb_value;
                }
                hasPendingUpdate = false; //update processed, no longer pending
                return; //return from the function after update processed
            }
        }
}

void initialize(){
/* Function: initialize
 * Sets and clears registers for desired operations on the MSP430
 */
    WDTCTL = WDTPW | WDTHOLD;               // Stop watchdog

    // Configure GPIO
    P1OUT &= ~BIT0;                         // Clear P1.0 output latch
    P1DIR |= BIT0;                          // For LED on P1.0
    //P1OUT &= ~BIT0;
    //P1OUT |= BIT0;
    //P6SEL1 &= ~(BIT0 | BIT1);
    //P6SEL0 |= (BIT0 | BIT1);                // USCI_A3 UART operation
    P2SEL1 & ~(BIT5 | BIT6);     // P2.5 to UCA1TXD and P2.6 to UCA1RXD
    P2SEL0 |= (BIT5 | BIT6);

    // Disable the GPIO power-on default high-impedance mode to activate
    // previously configured port settings
    PM5CTL0 &= ~LOCKLPM5;
    // Configure USCI_A1 for UART mode: baud rate 115200: 1 stop bit, no parity
        UCA1CTLW0 = UCSWRST;                    // Put eUSCI in reset
        UCA1CTLW0 |= UCSSEL__SMCLK;             // CLK = SMCLK
        UCA1BRW = 8;                            // 1000000/115200 = 8.68
        UCA1MCTLW = 0xD600;                     // 1000000/115200 - INT(1000000/115200)=0.68
                                                // UCBRSx value = 0xD6 (See UG)
        UCA1CTLW0 &= ~UCSWRST;                  // release from reset
        UCA1IE |= UCRXIE;                       // Enable USCI_A1 RX interrupt
    // Configure USCI_A3 for UART mode: baud rate 115200: 1 stop bit, no parity
    /*
    UCA3CTLW0 = UCSWRST;                    // Put eUSCI in reset
    UCA3CTLW0 |= UCSSEL__SMCLK;             // CLK = SMCLK
    UCA3BRW = 8;                            // 1000000/115200 = 8.68
    UCA3MCTLW = 0xD600;                     // 1000000/115200 - INT(1000000/115200)=0.68
                                            // UCBRSx value = 0xD6 (See UG)
    UCA3CTLW0 &= ~UCSWRST;                  // release from reset
    UCA3IE |= UCRXIE;                       // Enable USCI_A3 RX interrupt
    */
}

void initBluetoothLE(){
/* Function: initBluetoothLE
 * sends message to RN4020 to turn on echo, reset, clear connections, unbind
 */
    unsigned char stream[] = {'+','\r','R',',','1','\r','K','\r','U','\r','S','N',',','a','u','d','i','o',
    'b','e','a','m','e','r','\r'}; //initial message to send to the RN4020
    unsigned int i = 0; //index
    unsigned int streamLength = sizeof(stream)/sizeof(stream[0]); //size of initial message
    for(i = 0; i<streamLength; i++)
    {
        send_data(stream[i]); //send that character
    }
    __bis_SR_register(GIE);//LPM0_bits |
    P1OUT &= ~BIT0;                          // For LED on P1.0

}

void initADC(){
    /*
     * Function: initADC
     * Description: This functions initializes operation for the ADC12_B
     * P1.2 is configured as an input (A2)
     *
     *
     */
        WDTCTL = WDTPW | WDTHOLD;               // Stop WDT

        // Configure GPIO
        P1SEL1 |= BIT2; // set bit 2 P1.2 -> A2
        P1SEL0 |= BIT2; // set bit 2 P1.2 -> A2
        //P1SEL1 |= BIT3; // set bit 3 P1.3 -> A3 CHECK
        //P1SEL0 |= BIT3; // set bit 3 P1.3 -> A3 CHECK

        // Disable the GPIO power-on default high-impedance mode to activate
        // previously configured port settings
        PM5CTL0 &= ~LOCKLPM5;

        // Configure ADC12
        ADC12CTL0 = ADC12ON | ADC12SHT0_2;      // Turn on ADC12, set sampling time
        ADC12CTL1 = ADC12SHP;                   // Use sampling timer
        //ADC12MCTL0 = ADC12DIF_1 + ADC12INCH_2; // Differential mode, input+ = A2, input- = A3 CHECK
        ADC12MCTL0 = ADC12VRSEL_0 + ADC12INCH_2; //set input channel A2
        ADC12CTL2 |= ADC12RES_2;                 // 12 bit resolution
        ADC12CTL1 |= ADC12CONSEQ_2 + ADC12SSEL_2; //single channel converted repeatedly, MCLK
        ADC12CTL0 |= ADC12MSC;    // multiple conversions
        ADC12CTL0 |= ADC12PDIV_1; // predivide mclk by 4: 16MHz/4=4MHz
        ADC12CTL0 |= ADC12DIV_5; // divide by 6: 4MHz/6 = 667kHz (667kHz/(14clks per sample cycle) = 47.6kHz sample rate)

        ADC12CTL0 |= ADC12ENC;                  // Enable conversions
        ADC12CTL0 |= ADC12SC;               // Start conversion-software trigger
}

void read_ADC(){
    adc[ADC_index++] = (_q15)ADC12MEM0;   //save ADC12 read into array
    if(ADC_index>=ADC_BUFF_LEN){ //if the index exceeds buffer bounds
        ADC_index = 0; //reset index, break here for clean graph of input
    }
}

int main(void){
    //initialize(); //initialize the MSP430
    //initBluetoothLE(); //initialize BluetoothLE on RN4020
    initADC();  //initialize ADC12 for input data
    msp_lea_init(); //initialize low energy accelerator
    msp_status status;

    msp_fill_q15_params bass_fillParams;
    msp_biquad_cascade_df1_q15_params bass_df1Params;

    msp_fill_q15_params mid_fillParams;
    msp_biquad_cascade_df1_q15_params mid_df1Params;

    msp_fill_q15_params high_fillParams;
    msp_biquad_cascade_df1_q15_params high_df1Params;

    /* Initialize BASS filter coefficients. */
    memcpy(bass_filterCoeffs, FILTER_COEFFS_BASS, sizeof(bass_filterCoeffs));
    /* Initialize BASS filter coefficients*/
    bass_fillParams.length = sizeof(bass_states)/sizeof(_q15);
    bass_fillParams.value = 0;
    status = msp_fill_q15(&bass_fillParams, (void *)bass_states);
    msp_checkStatus(status);
    /* Initialize the parameter structure for BASS. */
    bass_df1Params.length = FILTER_LENGTH;
    bass_df1Params.stages = BASS_FILTER_STAGES;
    bass_df1Params.coeffs = bass_filterCoeffs;
    bass_df1Params.states = bass_states;
    /* Initialize scale of BASS */
    bass_scale.length = FILTER_LENGTH;
    bass_scale.scale = INT16_MAX/100 * equalization1_value; //scale is value/100
    bass_scale.shift = 0; // no shift
    /* Initialize scale of MID */
    mid_scale.length = FILTER_LENGTH;
    mid_scale.scale = INT16_MAX/100 * equalization2_value; //scale is value/100
    mid_scale.shift = 0; // no shift
    /* Initialize scale of HIGH */
    high_scale.length = FILTER_LENGTH;
    high_scale.scale = INT16_MAX/100 * equalization3_value; //scale is value/100
    high_scale.shift = 0; // no shift
    /* Initialize scale of REVERB */
    reverb_scale.length = FILTER_LENGTH;
    reverb_scale.scale = INT16_MAX/200 * reverb_value; //scale is value/100
    reverb_scale.shift = 0; // no shift

    /* Initialize MID filter coefficients. */
    memcpy(mid_filterCoeffs, FILTER_COEFFS_MID, sizeof(mid_filterCoeffs));
    /* Initialize MID filter coefficients*/
    mid_fillParams.length = sizeof(mid_states)/sizeof(_q15);
    mid_fillParams.value = 0;
    status = msp_fill_q15(&mid_fillParams, (void *)mid_states);
    msp_checkStatus(status);
    /* Initialize the parameter structure for MID. */
    mid_df1Params.length = FILTER_LENGTH;
    mid_df1Params.stages = MID_FILTER_STAGES;
    mid_df1Params.coeffs = mid_filterCoeffs;
    mid_df1Params.states = mid_states;


    /* Initialize HIGH filter coefficients. */
    memcpy(high_filterCoeffs, FILTER_COEFFS_HIGH, sizeof(high_filterCoeffs));
    /* Initialize HIGH filter coefficients*/
    high_fillParams.length = sizeof(high_states)/sizeof(_q15);
    high_fillParams.value = 0;
    status = msp_fill_q15(&high_fillParams, (void *)high_states);
    msp_checkStatus(status);
    /* Initialize the parameter structure for HIGH. */
    high_df1Params.length = FILTER_LENGTH;
    high_df1Params.stages = HIGH_FILTER_STAGES;
    high_df1Params.coeffs = high_filterCoeffs;
    high_df1Params.states = high_states;

    /* Parameter for vector addition */
    msp_add_q15_params add_params;
    add_params.length = FILTER_LENGTH;


    /* Parameter for vector copy */
    msp_copy_q15_params copy_params;
    copy_params.length = FILTER_LENGTH;

    while(1){
       // if(hasPendingUpdate){ //if a slider value was updated
       //     update_slider_value(); //update the corresponding value
       // }
        while(!(ADC12IFGR0 & BIT0));
        read_ADC();
        if(ADC_index == 0){
            /* Invoke the msp_biquad_cascade_df1_q15 function. */
            /* BASS */
            status = msp_biquad_cascade_df1_q15(&bass_df1Params, adc, bass_result); //filter bass
            msp_checkStatus(status);
            msp_scale_q15(&bass_scale, bass_result, bass_result); // scale bass
            /* MID */
            status = msp_biquad_cascade_df1_q15(&mid_df1Params, adc, mid_result); //filter mid
            msp_checkStatus(status);
            msp_scale_q15(&mid_scale, mid_result, mid_result); // scale mid
            /* HIGH */
            status = msp_biquad_cascade_df1_q15(&high_df1Params, adc, high_result); //filter high
            msp_checkStatus(status);
            msp_scale_q15(&high_scale, high_result, high_result); // scale high
            /* Sum scaled vectors */
            msp_add_q15(&add_params, bass_result, mid_result, eq_result); // add bass and mid
            msp_add_q15(&add_params, high_result, eq_result, eq_result); // add high result
            msp_checkStatus(status);
            /* REVERB */
            msp_scale_q15(&reverb_scale, prev_result, prev_result); // scale previous output
            msp_add_q15(&add_params, prev_result, eq_result, eq_result); // add previous output
            /* DISTORTION */
            //reverb_value is [0, 100]
            //HardClip value is (100 - reverb_value) * MAXINT
            int k; //count down
            for(k = FILTER_LENGTH; k>=0; k--){ // for each int16 in the eq_result output
                if(eq_result[k] > hardclip_value){ // if value > max
                    eq_result[k] = hardclip_value; // set to max
                }else if(eq_result[k] < -hardclip_value){ // if value < -max
                    eq_result[k] = -hardclip_value; // set to -max
                }
            }
            msp_copy_q15(&copy_params, eq_result, prev_result); // copy output to previous output
        }
    }
}

#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=EUSCI_A1_VECTOR
__interrupt void USCI_A1_ISR(void)
#elif defined(__GNUC__)
void _attribute__ ((interrupt(EUSCI_A1_VECTOR))) USCI_A1_ISR (void)
/* Function: BluetoothLE UART interrupt
 * Fills the value of the UART stream into the unsigned char buffer recvBufferTemp
*/
#endif
{
    switch(__even_in_range(UCA1IV, UCSTTIFG))
            {
                case USCI_UART_UCRXIFG: //flag for recv buffer full
                    recvBufferTemp[recvBufferTemp_Index++] = UCA1RXBUF; //store this value in a temp buffer
                    if(recvBufferTemp_Index>(RECV_STREAM_LENGTH-1))
                    {
                        recvBufferTemp_Index=0;
                    }
                    hasPendingUpdate = true; //tells main to look for an updated slider value
                    break;
                default: break;
            }
}

#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=EUSCI_A3_VECTOR
__interrupt void USCI_A3_ISR(void)
#elif defined(__GNUC__)
void _attribute__ ((interrupt(EUSCI_A3_VECTOR))) USCI_A3_ISR (void)
/* Function: BluetoothLE UART interrupt
 * Fills the value of the UART stream into the unsigned char buffer recvBufferTemp
*/
#endif
{
    switch(__even_in_range(UCA3IV, UCSTTIFG))
            {
                case USCI_UART_UCRXIFG: //flag for recv buffer full
                    recvBufferTemp[recvBufferTemp_Index++] = UCA3RXBUF; //store this value in a temp buffer
                    if(recvBufferTemp_Index>(RECV_STREAM_LENGTH-1))
                    {
                        recvBufferTemp_Index=0;
                    }
                    hasPendingUpdate = true; //tells main to look for an updated slider value
                    break;
                default: break;
            }
}
