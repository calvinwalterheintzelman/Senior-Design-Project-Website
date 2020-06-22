#include <msp430.h>

//http://www.ti.com/product/MSP430FR5994/toolssoftware#softTools

unsigned char RXData;
unsigned char TXData;
int x;
int counter;

int main(void)
{
    TXData = 0x32;
    WDTCTL = WDTPW | WDTHOLD;
    //WDTCTL = WDTPW | WDTTMSEL | WDTCNTCL | WDTIS__8192K;

    // Configure GPIO
    P1OUT &= ~BIT0;                         // Clear P1.0 output latch
    P1DIR |= BIT0;                          // For LED
    P7SEL0 |= BIT0 | BIT1;
    P7SEL1 &= ~(BIT0 | BIT1);

    P3DIR |= BIT0;
    P3OUT &= ~BIT0;


    CSCTL0_H = CSKEY_H;                     // Unlock CS registers
    CSCTL1 = DCOFSEL_6;                     // Set DCO = 8MHz
    CSCTL2 = SELA__VLOCLK | SELS__DCOCLK | SELM__DCOCLK; // Set ACLK = VLO, MCLK = DCO
    CSCTL3 = DIVA__1 | DIVS__8 | DIVM__8;   // Set all dividers
    CSCTL0_H = 0;



    // Disable the GPIO power-on default high-impedance mode to activate
    // previously configured port settings
    PM5CTL0 &= ~LOCKLPM5;


    // Configure USCI_B2 for I2C mode
    UCB2CTLW0 |= UCSWRST;                   // Software reset enabled
    UCB2CTLW0 |= UCMODE_3 | UCMST | UCSYNC | UCTR | UCTXACK; // I2C mode, Master mode, sync, transmitter
    UCB2CTLW1 |= UCASTP_2 | UCSTPNACK | UCSWACK;                  // Automatic stop generated
                                            // after UCB2TBCNT is reached

    //UCB2I2COA0 |= UCGCEN;               // CLOCK SHIT
    UCB2BRW = 0x24;                       // baudrate = SMCLK / 30; SMCLK is 16 MHz
    UCB2TBCNT = 0x000C;                     // number of bytes to be received
    UCB2I2CSA = 0x0070;                     // Slave address



    UCB2CTLW0 &= ~UCSWRST;

    UCB2IE |= UCNACKIE | UCSTTIE; //UCTXIE | UCRXIE | UCTXIE0 | UCRXIE0;



    //UCB2TXBUF = TXData;

    x = 0;
    //UCB2CTL1 |= UCTR | UCTXSTT;                // I2C start condition

    while(x+5 > 0) {
        __delay_cycles(5000);

        while (UCB2CTL1 & UCTXSTP);
        UCB2CTL1 |= UCTXSTT;
        __delay_cycles(5000);

        UCB2TXBUF = 0x00;
        __delay_cycles(5000);

        UCB2TXBUF = 0x18;
        __delay_cycles(5000);

        UCB2TXBUF = 0x02;
        __delay_cycles(5000);

        UCB2CTL1 |= UCTXSTP;
        x--;
    }


    x = 2000;
    counter = 0;

    PM5CTL0 &= ~LOCKLPM5;

    TA0CTL = TASSEL_2 | MC__CONTINUOUS | ID__8;
    TA0CTL &= ~(TAIFG_0);
    TA0CTL |= TAIE_1;

    __enable_interrupt();
    //SFRIE1 |= WDTIE;
    __bis_SR_register(LPM0_bits + GIE);     // Enter LPM0, enable interrupts
    //__no_operation();                       // For debugger

    while(1);
}

#pragma vector = TIMER0_A1_VECTOR
__interrupt void TIMERA_TIMER_OVERFLOW_ISR(void) {
    while (UCB2CTL1 & UCTXSTP);

    UCB2CTL1 |= UCTR;
    UCB2CTL1 |= UCTXSTT;
    __delay_cycles(5000);

    UCB2TXBUF = 0x00;
    __delay_cycles(5000);

    UCB2CTL1 &= ~UCTR;
    __delay_cycles(5000);

    UCB2CTL1 |= UCTXSTT;

    __delay_cycles(5000);


    RXData = UCB2RXBUF;
    __delay_cycles(5000);

    RXData = UCB2RXBUF;
    __delay_cycles(5000);

    RXData = UCB2RXBUF;
    __delay_cycles(5000);

    RXData = UCB2RXBUF;
    __delay_cycles(5000);

    RXData = UCB2RXBUF;
    __delay_cycles(5000);

    RXData = UCB2RXBUF;
    __delay_cycles(5000);

    RXData = UCB2RXBUF;
    __delay_cycles(5000);

    RXData = UCB2RXBUF;
    __delay_cycles(5000);

    RXData = UCB2RXBUF;
    __delay_cycles(5000);
    x = RXData; // voltage value 1 (little)

    RXData = UCB2RXBUF;
    __delay_cycles(5000);
    x = x + ((int)RXData)*16*16; // voltage value 2 (big)

    RXData = UCB2RXBUF;
    __delay_cycles(5000);

    RXData = UCB2RXBUF;
    __delay_cycles(5000);



    UCB2CTL1 |= UCTXSTP;
    UCB2CTL1 |= UCTXSTT;

    if(x < 1435){
        if(counter == 1) {
            P3OUT |= BIT0;
            counter = 0;
        } else {
            counter += 1;
        }
    }
    else {
        P3OUT &= ~BIT0;
        counter = 0;
    }
}

/*
// Watchdog Timer interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=WDT_VECTOR
__interrupt void WDT_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(WDT_VECTOR))) WDT_ISR (void)
#else
#error Compiler not supported!
#endif
{
    while (UCB2CTL1 & UCTXSTP);

    UCB2CTL1 |= UCTR;
    UCB2CTL1 |= UCTXSTT;
    __delay_cycles(5000);

    UCB2TXBUF = 0x00;
    __delay_cycles(5000);

    UCB2CTL1 &= ~UCTR;
    __delay_cycles(5000);

    UCB2CTL1 |= UCTXSTT;

    __delay_cycles(5000);


    RXData = UCB2RXBUF;
    __delay_cycles(5000);

    RXData = UCB2RXBUF;
    __delay_cycles(5000);

    RXData = UCB2RXBUF;
    __delay_cycles(5000);

    RXData = UCB2RXBUF;
    __delay_cycles(5000);

    RXData = UCB2RXBUF;
    __delay_cycles(5000);

    RXData = UCB2RXBUF;
    __delay_cycles(5000);

    RXData = UCB2RXBUF;
    __delay_cycles(5000);

    RXData = UCB2RXBUF;
    __delay_cycles(5000);

    RXData = UCB2RXBUF;
    __delay_cycles(5000);
    x = RXData; // voltage value 1 (little)

    RXData = UCB2RXBUF;
    __delay_cycles(5000);
    x = x + ((int)RXData)*16*16; // voltage value 2 (big)

    RXData = UCB2RXBUF;
    __delay_cycles(5000);

    RXData = UCB2RXBUF;
    __delay_cycles(5000);



    UCB2CTL1 |= UCTXSTP;
    UCB2CTL1 |= UCTXSTT;

    if(x < 1435){
        if(counter == 1) {
            P3OUT |= BIT0;
            counter = 0;
        } else {
            counter += 1;
        }
    }
    else {
        P3OUT &= ~BIT0;
        counter = 0;
    }
}*/

/*
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = EUSCI_B2_VECTOR
__interrupt void USCI_B2_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(EUSCI_B2_VECTOR))) USCI_B2_ISR (void)
#else
#error Compiler not supported!
#endif
{
    switch(__even_in_range(UCB2IV, USCI_I2C_UCBIT9IFG))
    {
        case USCI_NONE:
            break;     // Vector 0: No interrupts
        case USCI_I2C_UCALIFG:   break;     // Vector 2: ALIFG
        case USCI_I2C_UCNACKIFG:            // Vector 4: NACKIFG
            UCB2CTL1 |= UCTXSTP;     // I2C start condition
            __bic_SR_register_on_exit(LPM0_bits);

            break;
        case USCI_I2C_UCSTTIFG:
            __bic_SR_register_on_exit(LPM0_bits);
            break;     // Vector 6: STTIFG
        case USCI_I2C_UCSTPIFG:
            //while (UCB2CTL1 & UCTXSTP);
            __bic_SR_register_on_exit(LPM0_bits);
            break;     // Vector 8: STPIFG
        case USCI_I2C_UCRXIFG3:  break;     // Vector 10: RXIFG3
        case USCI_I2C_UCTXIFG3:  break;     // Vector 12: TXIFG3
        case USCI_I2C_UCRXIFG2:  break;     // Vector 14: RXIFG2
        case USCI_I2C_UCTXIFG2:  break;     // Vector 16: TXIFG2
        case USCI_I2C_UCRXIFG1:  break;     // Vector 18: RXIFG1
        case USCI_I2C_UCTXIFG1:  break;     // Vector 20: TXIFG1
        case USCI_I2C_UCRXIFG0:             // Vector 22: RXIFG0
            UCB2CTL1 |= TXData;
            break;
        case USCI_I2C_UCTXIFG0:

            //__bic_SR_register_on_exit(LPM0_bits);
            UCB2TXBUF = TXData;
            __bic_SR_register_on_exit(LPM0_bits);
            break;     // Vector 24: TXIFG0
        case USCI_I2C_UCBCNTIFG:            // Vector 26: BCNTIFG
            P1OUT ^= BIT0;                  // Toggle LED on P1.0
            break;
        case USCI_I2C_UCCLTOIFG: UCB2CTL1 |= UCTXSTP; break;     // Vector 28: clock low timeout
        case USCI_I2C_UCBIT9IFG: UCB2CTL1 |= UCTXSTP; break;     // Vector 30: 9th bit
        default: break;
    }
}*/

