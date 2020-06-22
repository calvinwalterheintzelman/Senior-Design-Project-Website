
/*
 * DSP INTEGRATION WITH WITH CODEC
 *
 * Author: Aditya Thagarthi Arun
 * Date created: 4/24/20
 * Date Modified: 5/3/20
 *
 */

//                   MSP430FR5994
//                 -----------------
//            /|\ |              XIN|-
//             |  |                 | 32KHz Crystal
//             ---|RST          XOUT|-
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
int distortion_value = 100;
int reverb_value = 100;

/* Hard Clipping Value */
//HardClip value is (100 - reverb_value)/100 * MAXINT
int hardclip_value = INT16_MAX - 27000;

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


_q15 absolute(_q15 number)
{
    if(number < 0)
    {
        return (number * -1);
    }
    else
    {
        return number;
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
    int distortion_scale;
    int b = ((distortion_value + 1) / 101) / 3.142;
    b = b * 2;
    b = b * (3.142 - (((distortion_value + 1) / 101) * (3.142 / 2)));
    distortion_scale = (2 * b) / (1 - b);


    int k; //count down
    for (k = 0; k < FILTER_LENGTH; k++)
    {
        eq_result[k] = (1 + distortion_scale) * (eq_result[k] / (1 + distortion_scale * absolute(eq_result[k])));
    }
//    for(k = FILTER_LENGTH - 1; k>=0; k--){ // for each int16 in the eq_result output
//    //    eq_result[k] = eq_result[k] * 2;
//        if(eq_result[k] > hardclip_value)
//
//
//        { // if value > max
//            eq_result[k] = hardclip_value; // set to max
//        }
//        else if(eq_result[k] < -hardclip_value)
//        { // if value < -max
//            eq_result[k] = -hardclip_value; // set to -max
//        }
//
//    }
    msp_copy_q15(&copy_params, eq_result, prev_result); // copy output to previous output

    //write from eqresult to codec_out_bufferA
    for(i=0;i<DSP_BUFF_LEN;i++)
    {
//        codec_out_bufferA[i<<2]=0;
//        codec_out_bufferA[(i<<2)+1]=0;

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
    int distortion_scale;
    int b = ((distortion_value + 1) / 101) / 3.142;
    b = b * 2;
    b = b * (3.142 - (((distortion_value + 1) / 101) * (3.142 / 2)));
    distortion_scale = (2 * b) / (1 - b);

    int k; //count down
    for (k = 0; k < FILTER_LENGTH; k++)
    {
       eq_result[k] = (1 + distortion_scale) * (eq_result[k] / (1 + distortion_scale * absolute(eq_result[k])));
    }

//    int k; //count down
//    for(k = FILTER_LENGTH - 1; k>=0; k--)
//    { // for each int16 in the eq_result output
//    //    eq_result[k] = eq_result[k] * 2;
//        if(eq_result[k] > hardclip_value)
//        { // if value > max
//            eq_result[k] = hardclip_value; // set to max
//        }
//        else if(eq_result[k] < -hardclip_value)
//        { // if value < -max
//            eq_result[k] = -hardclip_value; // set to -max
//        }
//
//     }

    msp_copy_q15(&copy_params, eq_result, prev_result); // copy output to previous output



    //write from eqresult to codec_out_bufferB
    for(i=0;i<DSP_BUFF_LEN;i++)
    {
//          codec_out_bufferB[4*i]=0;//(0xFF & (dsp_buffer[i]>>8));
//          codec_out_bufferB[4*i+1]=0;//(0xFF & dsp_buffer[i]);

          codec_out_bufferB[(i<<2)+2]= (0xFF & (eq_result[i]>>8));
          codec_out_bufferB[(i<<2)+3]= (0xFF & eq_result[i]);
    }

}



int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;               // Stop watch dog timer
    msp_lea_init();
    // Configure GPIO
    P6SEL1 &= ~(BIT0 | BIT1 | BIT2 | BIT3); // USCI_B1 SCLK, MOSI,
    P6SEL0 |= (BIT0 | BIT1 | BIT2 | BIT3);  // STE, and MISO pin
    P5OUT &= ~(BIT0);
    P5DIR |= BIT0;

    PJSEL0 |= BIT4 | BIT5;
    P3DIR |= BIT4;
    P3SEL1 |= BIT4;                         // Output system clock
    P3SEL0 |= BIT4;

    // Disable the GPIO power-on default high-impedance mode to activate
    // previously configured port settings
    PM5CTL0 &= ~LOCKLPM5;

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


    // SPI , I2S config
    // Master
    // 4 Pin SPI
    // 8 data bits
    // Clock Source: SMCLK (8 MHz)
    // Bit Rate Divider: 9 (for 1 MHz)
    // No Modulation
    // MSB First
    // Clock Phase - UCCKPL = 0, UCCKPH = 1
    // MSB first

    // Configure USCI_B1 for SPI operation
    UCA3CTLW0 |= UCSWRST;                    // **Put state machine in reset**
    UCA3CTLW0 |= UCCKPH| UCMSB | UCMST | UCMODE_1 | UCSYNC;
    UCA3CTLW0 &= ~(UCCKPL);
    UCA3CTLW0 |= UCSSEL__SMCLK;
    UCA3BRW = 0x08;  //scaling value for clock

    UCA3CTLW0 &= ~UCSWRST;                  // **Initialize USCI state machine**
    P5OUT |= BIT0;
    __bis_SR_register(GIE);
     __no_operation();

    /*
     * DMA config
     */
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

    while(1)
    {

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
