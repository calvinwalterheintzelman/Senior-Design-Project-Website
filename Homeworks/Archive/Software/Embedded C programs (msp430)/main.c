/*
 * Final audio beam project file
 *
 * Author: Aditya Thagarthi Arun, Aditya Biala, Calvin Heintzelman, Carson Tabachka
 * Date created: 4/24/20
 * Date Modified: 5/7/20
 *
 */

//                   MSP430FR5994
//                 -----------------
//            /|\ |              XIN|-
//             |  |                 | 32KHz Crystal
//             ---|RST          XOUT|-
//                |                 |
//                |             P3.0|-> LED output
//                |                 |
//                |             P3.4|-> SYS CLOCK
//                |                 |
//                |             P6.0|-> Data Out (UCA3SIMO)
//                |                 |
//                |             P6.1|<- Data In (UCA3SOMI)
//                |                 |
//                |             P6.2|-> Serial/Bit Clock (UCA3CLK)
//                |                 |
//                |             P6.3|-> Slave Select (UCA3STE)/CS
//                |                 |
//                |             P7.0|-> SDA for I2C
//                |                 |
//                |             P7.1|-> SCL for I2C
//                |                 |

//We got some help from these:
//https://www.ti.com/lit/ug/slau367p/slau367p.pdf?&ts=1588885378807
//http://www.ti.com/product/MSP430FR5994/toolssoftware#softTools

//P6.0-UCA3SIMO
//P6.1-UCA3SOMI
//P6.2/UCA3CLK
//P6.3/UCA3STE

#include <msp430.h>
#include <stdio.h>
#include <stdbool.h>

#include "include/bass.h"
#include "include/mid.h"
#include "include/high.h"
#include "include/all_pass.h"
#include "include/DSPLib.h"

#define DSP_BUFF_LEN 200

/* BLE GLOBAL VARIABELS */ //CODEC 2.0
#define RECV_STREAM_LENGTH 19

unsigned char recvBufferTemp[RECV_STREAM_LENGTH] = {0};
unsigned int recvBufferTemp_Index = 0;
bool hasPendingUpdate = false;



/* Bass Filter Parameters */
#define FILTER_LENGTH (DSP_BUFF_LEN)
#define BASS_FILTER_STAGES (sizeof(FILTER_COEFFS_BASS)/sizeof(msp_biquad_df1_q15_coeffs))


/* MID Filter Parameters */
#define FILTER_LENGTH (DSP_BUFF_LEN)
#define MID_FILTER_STAGES (sizeof(FILTER_COEFFS_MID)/sizeof(msp_biquad_df1_q15_coeffs))


/* HIGH Filter Parameters */
#define FILTER_LENGTH (DSP_BUFF_LEN)
#define HIGH_FILTER_STAGES (sizeof(FILTER_COEFFS_HIGH)/sizeof(msp_biquad_df1_q15_coeffs))


int sendpacket=0;

volatile char codec_in_bufferA[4*DSP_BUFF_LEN]; // Audio from Codec (4 bytes per sample)
volatile char codec_in_bufferB[4*DSP_BUFF_LEN]; // Audio from Codec (4 bytes per sample)
volatile char codec_out_bufferA[4*DSP_BUFF_LEN]; // Audio to Codec (4 bytes per sample)
volatile char codec_out_bufferB[4*DSP_BUFF_LEN]; // Audio to Codec (4 bytes per sample)

char act_codec_in_bufr = 0; // active codec_in buffer (0 | 1)
char act_codec_out_bufr = 0; // active codec_out buffer (0 | 1)

///////////// DSP GLOBALS start //////////////////////
/* Filter input signal */
DSPLIB_DATA(dsp_buffer,4)
_q15 dsp_buffer[DSP_BUFF_LEN] = {0}; // array of ADC transfered values

int equalization1_value = 100;
int equalization2_value = 100;
int equalization3_value = 100;
int distortion_value = 0;
int reverb_value = 0;

//PMIC variables
unsigned char RXData;
int x;

/* Hard Clipping Value */
//HardClip value is (100 - reverb_value)/100 * MAXINT
int hardclip_value = INT16_MAX;

/* Bass Filter result */
DSPLIB_DATA(bass_result,4)
_q15 bass_result[DSP_BUFF_LEN];


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
_q15 mid_result[DSP_BUFF_LEN];


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
_q15 high_result[DSP_BUFF_LEN];


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
_q15 eq_result[DSP_BUFF_LEN];

/* Previous Output */
DSPLIB_DATA(prev_result,4)
_q15 prev_result[DSP_BUFF_LEN];


msp_status status;

msp_fill_q15_params bass_fillParams;
msp_biquad_cascade_df1_q15_params bass_df1Params;

msp_fill_q15_params mid_fillParams;
msp_biquad_cascade_df1_q15_params mid_df1Params;

msp_fill_q15_params high_fillParams;
msp_biquad_cascade_df1_q15_params high_df1Params;



/* Parameter for vector addition */
msp_add_q15_params add_params;


/* Parameter for vector copy */
msp_copy_q15_params copy_params;

// distortion algorithm constants init


void send_data (unsigned char data)
{
    while(!(UCA3IFG & UCTXIFG));  //wait until tx buffer is empty before loading new data
    UCA3TXBUF = data;
}

int hexToInt(char c)
{ //convert single char into int
    if('0'<=c && c<='9')
    { //chars '0'-'9'
        return (int) c - '0';
    }
    if('A'<=c && c<='F')
    { //chars 'A' - 'F'
        return (int) c - 'A';
    }
    return 0;
}

void update_slider_value()
{
/* Function: update_slider_value
 * This function is called when 'hasPendingUpdate' is set to true
 * Scans the recvBufferTemp for the update message and updates the corresponding value accordingly
 *
 */
    unsigned int msg_index;
    for(msg_index = 0; msg_index < RECV_STREAM_LENGTH; msg_index++)
    { //for every character in the recv stream
        if(recvBufferTemp[msg_index] == 'N')
        { //if this character is 'N'
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
                hardclip_value = (200 - distortion_value) * (INT16_MAX / 200);
            }else if(msg_ID[1] == '2'){
                reverb_value = hexValue;
                reverb_scale.scale = INT16_MAX/200 * reverb_value;
            }
            hasPendingUpdate = false; //update processed, no longer pending
            return; //return from the function after update processed
        }
    }
}


void initBluetoothLE()
{
/* Function: initBluetoothLE
 * sends message to RN4020 to turn on echo, reset, clear connections, unbind
 */
    unsigned char stream[] = {'+','\r','R',',','1','\r','K','\r','U','\r','S','N',',','a','u','d','i','o',
    'b','e','a','m','e','r','\r','S','R',',','2','4','0','6','0','0','0','0','\r','R',',','1','\r'}; //initial message to send to the RN4020
    unsigned int i = 0; //index
    unsigned int streamLength = sizeof(stream)/sizeof(stream[0]); //size of initial message
    for(i = 0; i<streamLength; i++)
    {
        send_data(stream[i]); //send that character
    }

}



//////////// DSP GLOBALS end ////////////////////////


void DSP_A()
{
    //copy into dsp_buffer from codec_in_bufferA
    int i=0;

    for(i=0;i<DSP_BUFF_LEN;i++)
    {
        dsp_buffer[i]=0;
        dsp_buffer[i] |= (codec_in_bufferA[(i<<2)+2] <<8);
        dsp_buffer[i] |= (codec_in_bufferA[(i<<2) +3]);

    }

    /* Invoke the msp_biquad_cascade_df1_q15 function. */
    /* BASS */
    status = msp_biquad_cascade_df1_q15(&bass_df1Params, dsp_buffer, bass_result); //filter bass
    msp_checkStatus(status);
    msp_scale_q15(&bass_scale, bass_result, bass_result); // scale bass
    /* MID */
    status = msp_biquad_cascade_df1_q15(&mid_df1Params, dsp_buffer, mid_result); //filter mid
    msp_checkStatus(status);
    msp_scale_q15(&mid_scale, mid_result, mid_result); // scale mid
    /* HIGH */
    status = msp_biquad_cascade_df1_q15(&high_df1Params, dsp_buffer, high_result); //filter high
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
    for(k = FILTER_LENGTH - 1; k>=0; k--) // for each int16 in the eq_result output
    {

        if(eq_result[k] > hardclip_value)
        {
            // if value > max
            eq_result[k] = hardclip_value; // set to max
        }
        else if(eq_result[k] < -hardclip_value)
        {
            // if value < -max
            eq_result[k] = -hardclip_value; // set to -max
        }

    }
    msp_copy_q15(&copy_params, dsp_buffer, prev_result); // copy output to previous output

    //write from eqresult to codec_out_bufferA
    for(i=0;i<DSP_BUFF_LEN;i++)
    {
        codec_out_bufferA[(i<<2)+2]=(0xFF & (eq_result[i]>>8));
        codec_out_bufferA[(i<<2)+3]=(0xFF & eq_result[i]);
    }

}



void DSP_B()
{

    // copy into dsp_buffer from codec_in_bufferB
    int i=0;
    for(i=0;i<DSP_BUFF_LEN;i++)
    {
        dsp_buffer[i]=0;
        dsp_buffer[i] |= (codec_in_bufferB[(i<<2)+2] <<8);
        dsp_buffer[i] |= (codec_in_bufferB[(i<<2) +3]);
    }


    /* Invoke the msp_biquad_cascade_df1_q15 function. */
    /* BASS */
    status = msp_biquad_cascade_df1_q15(&bass_df1Params, dsp_buffer, bass_result); //filter bass
    msp_checkStatus(status);
    msp_scale_q15(&bass_scale, bass_result, bass_result); // scale bass
    /* MID */
    status = msp_biquad_cascade_df1_q15(&mid_df1Params, dsp_buffer, mid_result); //filter mid
    msp_checkStatus(status);
    msp_scale_q15(&mid_scale, mid_result, mid_result); // scale mid
    /* HIGH */
    status = msp_biquad_cascade_df1_q15(&high_df1Params, dsp_buffer, high_result); //filter high
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
    for(k = FILTER_LENGTH - 1; k>=0; k--)// for each int16 in the eq_result output
    {
        if(eq_result[k] > hardclip_value)
        {
            // if value > max
            eq_result[k] = hardclip_value; // set to max
        }
        else if(eq_result[k] < -hardclip_value)
        {
            // if value < -max
            eq_result[k] = -hardclip_value; // set to -max
        }

     }

    msp_copy_q15(&copy_params, dsp_buffer, prev_result); // copy output to previous output


    //write from eqresult to codec_out_bufferB
    for(i=0;i<DSP_BUFF_LEN;i++)
    {
        codec_out_bufferB[(i<<2)+2]= (0xFF & (eq_result[i]>>8));
        codec_out_bufferB[(i<<2)+3]= (0xFF & eq_result[i]);
    }

}


void gpio_init()
{

   // P6.0 to 6.3 for SPI
   P6SEL1 &= ~(BIT0 | BIT1 | BIT2 | BIT3); // SCLK, MOSI,
   P6SEL0 |= (BIT0 | BIT1 | BIT2 | BIT3);  // STE, and MISO pin

   // P2.5 to UCA1TXD and P2.6 to UCA1RXD for UART
   P2SEL1 & ~(BIT5 | BIT6);
   P2SEL0 |= (BIT5 | BIT6);

   //P5.0 for reset bit for counter in i2s
   P5OUT &= ~(BIT0);
   P5DIR |= BIT0;

   PJSEL0 |= BIT4 | BIT5;

   // Output system clock to codec
   P3DIR |= BIT4;
   P3SEL1 |= BIT4;
   P3SEL0 |= BIT4;

   // Configure GPIO
   P7SEL0 |= BIT0 | BIT1;
   P7SEL1 &= ~(BIT0 | BIT1);

   P3DIR |= BIT0;
   P3OUT &= ~BIT0;

   // Disable the GPIO power-on default high-impedance mode to activate
   // previously configured port settings
   PM5CTL0 &= ~LOCKLPM5;
}


int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;               // Stop watch dog timer
    msp_lea_init();

    gpio_init();// initializes all gpio pins required


    // XT1 Setup
    CSCTL0_H = CSKEY_H;                     // Unlock CS registers
    CSCTL1 = DCOFSEL_6;                     // Set DCO to 8MHz
    CSCTL1 &= ~(DCORSEL);
    CSCTL2 = SELA__LFXTCLK | SELS__DCOCLK | SELM__DCOCLK;
    CSCTL3 = DIVA__4 | DIVS__4 | DIVM__4;//errata management
    CSCTL3 = DIVA__1 | DIVS__1 | DIVM__1;   // Set all dividers
    CSCTL4 &= ~LFXTOFF;


    do
    {
        CSCTL5 &= ~LFXTOFFG;                // Clear XT1 fault flag
        SFRIFG1 &= ~OFIFG;
    } while (SFRIFG1 & OFIFG);              // Test oscillator fault flag
    CSCTL0_H = 0;                           // Lock CS registers


    /*
     * DSP Initialiazion
     */

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

    add_params.length = FILTER_LENGTH;
    copy_params.length = FILTER_LENGTH;


    // SPI , I2S config ==> UCA3
    // Master
    // 4 Pin SPI
    // 8 data bits
    // Clock Source: SMCLK (8 MHz)
    // Bit Rate Divider: 9 (for 1 MHz)
    // No Modulation
    // MSB First
    // Clock Phase - UCCKPL = 0, UCCKPH = 1
    // MSB first
    UCA3CTLW0 |= UCSWRST;                    // **Put state machine in reset**
    UCA3CTLW0 |= UCCKPH| UCMSB | UCMST | UCMODE_1 | UCSYNC;
    UCA3CTLW0 &= ~(UCCKPL);
    UCA3CTLW0 |= UCSSEL__SMCLK;
    UCA3BRW = 0x08;  //scaling value for clock
    UCA3CTLW0 &= ~UCSWRST;                  // **Initialize USCI state machine**
    P5OUT |= BIT0;


    // Configure USCI_A1 for UART mode: baud rate 115200: 1 stop bit, no parity
    UCA1CTLW0 = UCSWRST;                    // Put eUSCI in reset
    UCA1CTLW0 |= UCSSEL__SMCLK;             // CLK = SMCLK
    UCA1BRW = 69;                            // 8000000/115200 = 69.44
    UCA1MCTLW = 0x5500;                     // 8000000/115200 - INT(8000000/115200)=0.444 => UCBRSx value = 0x55 (See UG page:779)
    UCA1CTLW0 &= ~UCSWRST;                  // release from reset
    UCA1IE |= UCRXIE;


    initBluetoothLE(); //initialize BluetoothLE on RN4020

    __bis_SR_register(GIE);
     __no_operation();


    // Set up the DMA Controller.
    //    DMA3-TX-UCA3TXIFG trigger
    //    DMA4-RX-UCA3RXIFG trigger
    DMACTL1 |= DMA3TSEL_17;// according to trigger number off UCA3TXIFG
    DMACTL2 |= DMA4TSEL_16;// according to trigger number off UCA3RXIFG

    //dma3 setup-tx
    DMA3CTL = DMADT_4 + DMADSTBYTE + DMASRCBYTE + DMAIE + DMAEN+DMASRCINCR_3+ DMADSTINCR_0;
    __data16_write_addr((unsigned short) &DMA3SA,(unsigned long) &codec_out_bufferA);
    __data16_write_addr((unsigned short) &DMA3DA,(unsigned long) &UCA3TXBUF);
    DMA3SZ = 4*DSP_BUFF_LEN;

    //dma4 setup-rx
    DMA4CTL = DMADT_4 | DMADSTINCR_3 | DMASRCINCR_0 | DMASBDB| DMAEN;
     __data16_write_addr((unsigned short) &DMA4SA,(unsigned long) &UCA3RXBUF);
    __data16_write_addr((unsigned short) &DMA4DA,(unsigned long) &codec_in_bufferA);
    DMA4SZ = 4*DSP_BUFF_LEN;

    //To trigger bursts of activity
    UCA3IFG &= ~(UCTXIFG);
    UCA3IFG |= UCTXIFG;


    // Configure USCI_B2 for I2C mode for PMIC
    UCB2CTLW0 |= UCSWRST;                   // Software reset enabled
    UCB2CTLW0 |= UCMODE_3 | UCMST | UCSYNC | UCTR | UCTXACK; // I2C mode, Master mode, sync, transmitter
    UCB2CTLW1 |= UCASTP_2 | UCSTPNACK | UCSWACK;                  // Automatic stop generated
                                            // after UCB2TBCNT is reached

    //UCB2I2COA0 |= UCGCEN;               // CLOCK SHIT
    UCB2BRW = 0x24;                       // baudrate = SMCLK / 30; SMCLK is 16 MHz
    UCB2TBCNT = 0x000C;                     // number of bytes to be received
    UCB2I2CSA = 0x0070;                     // Slave address


    UCB2CTLW0 &= ~UCSWRST;

    UCB2IE |= UCNACKIE | UCSTTIE;
    x = 0;

    while(x+5 > 0) {

        while (UCB2CTL1 & UCTXSTP);
        UCB2CTL1 |= UCTXSTT;
        while (UCB2CTL1 & UCTXSTT);

        UCB2TXBUF = 0x00;
        while (!(UCB2IFG & UCTXIFG));

        UCB2TXBUF = 0x18;
        while (!(UCB2IFG & UCTXIFG));

        UCB2TXBUF = 0x02;
        while (!(UCB2IFG & UCTXIFG));

        UCB2CTL1 |= UCTXSTP;
        x--;
    }


    x = 2000;

    PM5CTL0 &= ~LOCKLPM5;

    TA0CTL = TASSEL_1 | MC__CONTINUOUS | ID_1; // increase the value of 1 in ID_1 to slow down PMIC checks
    TA0CTL &= ~(TAIFG_0);
    TA0CTL |= TAIE_1;

    __enable_interrupt();

    while(1)
    {
        if(hasPendingUpdate)
        {
            //if a slider value was updated for bluetooth
            update_slider_value(); //update the corresponding value
        }
    }

}

#pragma vector=DMA_VECTOR
__interrupt void DMA_ISR(void)
{
    P1DIR |= 0x01;
    P1OUT ^= 0x01;

    DMA3CTL &= ~DMAIFG;
    if (act_codec_in_bufr == 0)
    {
        // Destination block address
        DSP_A();
        __data16_write_addr((unsigned short) &DMA4DA, (unsigned long) &codec_in_bufferA);

    }

    else
    {
        // Destination block address
       DSP_B();
        __data16_write_addr((unsigned short) &DMA4DA, (unsigned long) &codec_in_bufferB);

    }

    act_codec_in_bufr ^= 0x01; // switch bufferr

    if (act_codec_out_bufr == 0)
    {
       //Source block address
        __data16_write_addr((unsigned short) &DMA3SA, (unsigned long) &codec_out_bufferA);
    }
    else
    {
        // Source block address
        __data16_write_addr((unsigned short) &DMA3SA, (unsigned long) &codec_out_bufferB);
    }

    act_codec_out_bufr ^= 0x01; // switch buffers

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

#pragma vector = TIMER0_A1_VECTOR
__interrupt void TIMERA_TIMER_OVERFLOW_ISR(void)
{
    while (UCB2CTL1 & UCTXSTP);

    UCB2CTL1 |= UCTR;
    while (!(UCB2CTL1 & UCTR));

    UCB2CTL1 |= UCTXSTT;
    while (UCB2CTL1 & UCTXSTT);


    UCB2TXBUF = 0x00;
    while (!(UCB2IFG & UCTXIFG));

    UCB2CTL1 &= ~UCTR;
    while (UCB2CTL1 & UCTR);

    UCB2CTL1 |= UCTXSTT;

    while (UCB2CTL1 & UCTXSTT);


    RXData = UCB2RXBUF;
    while (!(UCB2IFG & UCRXIFG));

    RXData = UCB2RXBUF;
    while (!(UCB2IFG & UCRXIFG));

    RXData = UCB2RXBUF;
    while (!(UCB2IFG & UCRXIFG));

    RXData = UCB2RXBUF;
    while (!(UCB2IFG & UCRXIFG));

    RXData = UCB2RXBUF;
    while (!(UCB2IFG & UCRXIFG));

    RXData = UCB2RXBUF;
    while (!(UCB2IFG & UCRXIFG));

    RXData = UCB2RXBUF;
    while (!(UCB2IFG & UCRXIFG));

    RXData = UCB2RXBUF; //7th
    while (!(UCB2IFG & UCRXIFG));

    RXData = UCB2RXBUF; //8th
    while (!(UCB2IFG & UCRXIFG));

    RXData = UCB2RXBUF; //9th
    while (!(UCB2IFG & UCRXIFG));
    x = (int)RXData; // voltage value 1 (little)

    RXData = UCB2RXBUF;
    while (!(UCB2IFG & UCRXIFG));
    x = x + ((int)RXData)*16*16; // voltage value 2 (big)


    RXData = UCB2RXBUF;
    while (!(UCB2IFG & UCRXIFG));

    UCB2CTL1 |= UCTXSTP;
    while (UCB2CTL1 & UCTXSTP);

    //read
    UCB2CTL1 |= UCTXSTT;
    while (UCB2CTL1 & UCTXSTT);

    UCB2CTL1 |= UCTXSTP;
    while (UCB2CTL1 & UCTXSTP);


    //write
    UCB2CTL1 |= UCTR;
    while (!(UCB2CTL1 & UCTR));

    UCB2CTL1 |= UCTXSTT;
    while (UCB2CTL1 & UCTXSTT);

    UCB2CTL1 |= UCTXSTP;
    while (UCB2CTL1 & UCTXSTP);

    //read
    UCB2CTL1 &= ~UCTR;
    while (UCB2CTL1 & UCTR);

    UCB2CTL1 |= UCTXSTT;
    while (UCB2CTL1 & UCTXSTT);

    UCB2CTL1 |= UCTXSTP;
    while (UCB2CTL1 & UCTXSTP);

    //read

    UCB2CTL1 |= UCTXSTT;
    while (UCB2CTL1 & UCTXSTT);

    UCB2CTL1 |= UCTXSTP;
    while (UCB2CTL1 & UCTXSTP);

    if(x < 1465){
        P3OUT |= BIT0;
    }
    else {
        P3OUT &= ~BIT0;
    }
}
