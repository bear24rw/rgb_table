#include <msp430.h>
#include <legacymsp430.h>

#define RED_FET BIT5
#define GRN_FET BIT3
#define BLU_FET BIT4

#define XLAT    BIT2
#define SIN     BIT1
#define SCLK    BIT0
#define BLANK   BIT0
#define VPRG    BIT3
#define GSCLK   BIT4

#define RED 0
#define GRN 1
#define BLU 2

#define NUM_LEDS (8*16*3)

// [R0, G0, B0, R1, G1, B1, R2, G2, ...]
volatile unsigned char table[NUM_LEDS];

volatile unsigned char color = RED;

volatile unsigned char need_xlat = 0;
unsigned char need_extra_sclk = 0;

unsigned int i;

void init_table()
{
    unsigned char r,g,b;

    for (i=0; i<NUM_LEDS; i+=3)
    {
        if (i < (1*16*3) ) {r = 255; g = 000; b = 000;}
        else if (i < (2*16*3) ) {r = 000; g = 255; b = 000;}
        else if (i < (3*16*3) ) {r = 000; g = 000; b = 255;}
        else if (i < (4*16*3) ) {r = 255; g = 255; b = 000;}
        else if (i < (5*16*3) ) {r = 255; g = 000; b = 255;}
        else if (i < (6*16*3) ) {r = 000; g = 255; b = 255;}
        else if (i < (7*16*3) ) {r = 255; g = 255; b = 255;}
        else if (i < (8*16*3) ) {r = 255; g = 255; b = 255;}

        table[i+RED] = (unsigned char)r;
        table[i+GRN] = (unsigned char)g;
        table[i+BLU] = (unsigned char)b;
    }
}

void init_dc()
{
    // enter DC Data Input Mode
    P1OUT |= VPRG;

    // set everything to 100%
    P2OUT |= SIN;
    
    // DC register is 96 bits, we have 8 chips, extra 5 just in case
    for (i=0; i<(96*8+5); i++)
    {
        P2OUT |= SCLK;
        P2OUT &= ~SCLK;
    }
    P2OUT &= ~SIN;

    // latch in the data
    P2OUT |= XLAT;
    P2OUT &= ~XLAT;

    // switch back to GS mode
    P1OUT &= ~VPRG;
}

void next_color()
{
    switch (color)
    {
        case RED:
            P2OUT |= GRN_FET;
            P2OUT |= BLU_FET;
            P2OUT &= ~RED_FET;
            color = GRN;
            break;
        case GRN:
            P2OUT |= RED_FET;
            P2OUT |= BLU_FET;
            P2OUT &= ~GRN_FET;
            color = BLU;
            break;
        case BLU:
            P2OUT |= RED_FET;
            P2OUT |= GRN_FET;
            P2OUT &= ~BLU_FET;
            color = RED;
            break;
        default:
            P2OUT |= RED_FET;
            P2OUT |= GRN_FET;
            P2OUT |= BLU_FET;
            color = RED;
            break;
    }
}

void fets_off()
{
    P2OUT |= RED_FET;
    P2OUT |= GRN_FET;
    P2OUT |= BLU_FET;
}

void send_data()
{

    // if were waiting for a latch dont update again
    if (need_xlat == 0)
    {

        if (need_extra_sclk)
        {
            P2OUT |= SCLK;
            P2OUT &= ~SCLK;
        }
        else
            need_extra_sclk = 1;

        for (i=0; i<NUM_LEDS; i+=3)
        {
            // 4 pad bits
            P2OUT &= ~SIN;

            P2OUT |= SCLK;
            P2OUT &= ~SCLK;

            P2OUT |= SCLK;
            P2OUT &= ~SCLK;

            P2OUT |= SCLK;
            P2OUT &= ~SCLK;

            P2OUT |= SCLK;
            P2OUT &= ~SCLK;

            // 8 data bits
            if (table[i+color] & (1 << 7)) { P2OUT |= SIN; } else { P2OUT &= ~SIN; }

            P2OUT |= SCLK;
            P2OUT &= ~SCLK;

            if (table[i+color] & (1 << 6)) { P2OUT |= SIN; } else { P2OUT &= ~SIN; }

            P2OUT |= SCLK;
            P2OUT &= ~SCLK;

            if (table[i+color] & (1 << 5)) { P2OUT |= SIN; } else { P2OUT &= ~SIN; }

            P2OUT |= SCLK;
            P2OUT &= ~SCLK;

            if (table[i+color] & (1 << 4)) { P2OUT |= SIN; } else { P2OUT &= ~SIN; }

            P2OUT |= SCLK;
            P2OUT &= ~SCLK;

            if (table[i+color] & (1 << 3)) { P2OUT |= SIN; } else { P2OUT &= ~SIN; }

            P2OUT |= SCLK;
            P2OUT &= ~SCLK;

            if (table[i+color] & (1 << 2)) { P2OUT |= SIN; } else { P2OUT &= ~SIN; }

            P2OUT |= SCLK;
            P2OUT &= ~SCLK;

            if (table[i+color] & (1 << 1)) { P2OUT |= SIN; } else { P2OUT &= ~SIN; }

            P2OUT |= SCLK;
            P2OUT &= ~SCLK;

            if (table[i+color] & (1 << 0)) { P2OUT |= SIN; } else { P2OUT &= ~SIN; }

            P2OUT |= SCLK;
            P2OUT &= ~SCLK;
        }

        need_xlat = 1;
    }
}

int main(void)
{

    WDTCTL = WDTPW + WDTHOLD;   // Stop WDT

    //
    // clock
    //

    BCSCTL1 = CALBC1_12MHZ;
    DCOCTL = CALDCO_12MHZ;
    //BCSCTL3 |= LFXT1S_2;

    //
    // UART
    //
    
    // http://www.daycounter.com/Calculators/MSP430-Uart-Calculator.phtml

    P1SEL |= (BIT1 | BIT1);     // P1.2 / P1.1 = TX / RX
    P1DIR |= BIT2;
    P1DIR &= ~BIT1;

    UCA0CTL1 |= UCSSEL_2;       // SMCLK
    UCA0BR0 = 0x18;             // 12MHz 500000 baud
    UCA0BR1 = 0x00;             // 12MHz 500000 baud
    UCA0MCTL = 0x00;            // 12Mhz 500000 baud
    UCA0CTL1 &= ~UCSWRST;       // initialize USCI state machine
    IE2 |= UCA0RXIE;            // enable USCI_A0 RX interrupt

    //
    // FETS
    //

    P2DIR |= RED_FET;
    P2DIR |= GRN_FET;
    P2DIR |= BLU_FET;

    fets_off();
    
    //
    // TLC5941
    //

    // set as outputs
    P2DIR |= XLAT;
    P2DIR |= SIN;
    P2DIR |= SCLK;
    P1DIR |= BLANK;
    P1DIR |= VPRG;
    P1DIR |= GSCLK;

    // SMCLK
    P1SEL |= GSCLK;

    // set to 0
    P2OUT &= ~XLAT;
    P2OUT &= ~SIN;
    P2OUT &= ~SCLK;
    P1OUT &= ~BLANK;
    P1OUT &= ~VPRG;
    P1OUT &= ~GSCLK;
    
    // setup timers
    TA0CCTL0 = OUTMOD_3 | CCIE;
    TA0CCR0 = 256;
    TA0CTL = TASSEL_2 | MC_1;       // SMCLK | Up mode

    init_dc();

    _EINT();

    init_table();

    while(1) 
    {
       send_data();
    }

    return 0;
}

unsigned char blank_count = 0;

// BLANK ISR
interrupt (TIMER0_A0_VECTOR) Timer_A(void)
{
    // pull BLANK high to reset the GS counter
    P1OUT |= BLANK;

    // check if we have new data that needs to be latched
    if (need_xlat)
    {
        // latch all the new data
        P2OUT |= XLAT;
        P2OUT &= ~XLAT;
        need_xlat = 0;
        next_color();
    }

    // enable the GS counter
    P1OUT &= ~BLANK;

    // reset the timer since it's been running during the ISR
    TA0CTL |= TACLR;
}

// UART RX ISR
interrupt (USCIAB0RX_VECTOR) uart_rx(void)
{
    static unsigned int rx_pointer = 0;
    static unsigned char rx_byte = 0;

    rx_byte = UCA0RXBUF;

    // check if we recieved a start byte
    if (rx_byte == 0xFF)
    {
        rx_pointer = 0;
    }
    else
    {
        table[rx_pointer] = UCA0RXBUF; 
        rx_pointer++;

        // sanity check, we should never hit end of the array
        if (rx_pointer == NUM_LEDS)
        {
            rx_pointer = 0;
        }
    }
}
