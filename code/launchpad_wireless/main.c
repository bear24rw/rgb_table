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

#define SPI_CLK     BIT5
#define SPI_SOMI    BIT6
#define SPI_SIMO    BIT7

#define RED 0
#define GRN 1
#define BLU 2

unsigned char table[8][16][3];

unsigned char state = 0;
unsigned char col,row;
unsigned char bit;

unsigned char color = RED;

unsigned char need_xlat = 0;
unsigned char need_extra_sclk = 0;

unsigned char r,g,b;

void init_table()
{
    for (row=0; row<8; row++)
    {
        if (row == 0 ) r = 200; g = 000; b = 000;
        if (row == 1 ) r = 000; g = 200; b = 000;
        if (row == 2 ) r = 000; g = 000; b = 200;
        if (row == 3 ) r = 200; g = 200; b = 000;
        if (row == 4 ) r = 200; g = 000; b = 200;
        if (row == 5 ) r = 000; g = 200; b = 200;
        if (row == 6 ) r = 200; g = 200; b = 200;
        if (row == 7 ) r = 200; g = 200; b = 200;

        for (col=0; col<16; col++)
        {
            table[row][col][RED] = (unsigned char)r;
            table[row][col][GRN] = (unsigned char)g;
            table[row][col][BLU] = (unsigned char)b;
        }
    }
}

void init_dc()
{
    // enter DC Data Input Mode
    P1OUT |= VPRG;

    // set everything to 100%
    P2OUT |= SIN;
    
    // DC register is 96 bits, we have 8 chips, extra 5 just in case
    int i;
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

        for (row=0; row<8; row++)
        {
            for (col=0; col<16; col++)
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
                if (table[row][col][color] & (1 << 7)) { P2OUT |= SIN; } else { P2OUT &= ~SIN; }

                P2OUT |= SCLK;
                P2OUT &= ~SCLK;

                if (table[row][col][color] & (1 << 6)) { P2OUT |= SIN; } else { P2OUT &= ~SIN; }

                P2OUT |= SCLK;
                P2OUT &= ~SCLK;

                if (table[row][col][color] & (1 << 5)) { P2OUT |= SIN; } else { P2OUT &= ~SIN; }

                P2OUT |= SCLK;
                P2OUT &= ~SCLK;

                if (table[row][col][color] & (1 << 4)) { P2OUT |= SIN; } else { P2OUT &= ~SIN; }

                P2OUT |= SCLK;
                P2OUT &= ~SCLK;

                if (table[row][col][color] & (1 << 3)) { P2OUT |= SIN; } else { P2OUT &= ~SIN; }

                P2OUT |= SCLK;
                P2OUT &= ~SCLK;

                if (table[row][col][color] & (1 << 2)) { P2OUT |= SIN; } else { P2OUT &= ~SIN; }

                P2OUT |= SCLK;
                P2OUT &= ~SCLK;

                if (table[row][col][color] & (1 << 1)) { P2OUT |= SIN; } else { P2OUT &= ~SIN; }

                P2OUT |= SCLK;
                P2OUT &= ~SCLK;

                if (table[row][col][color] & (1 << 0)) { P2OUT |= SIN; } else { P2OUT &= ~SIN; }

                P2OUT |= SCLK;
                P2OUT &= ~SCLK;
            }
        }

        need_xlat = 1;
    }
}

int main(void)
{

    WDTCTL = WDTPW + WDTHOLD;   // Stop WDT

    BCSCTL1 = CALBC1_12MHZ;
    DCOCTL = CALDCO_12MHZ;
    //BCSCTL3 |= LFXT1S_2;

    // SPI

    P1SEL |= SPI_CLK;
    P1SEL |= SPI_SOMI;
    P1SEL |= SPI_SIMO;

    P1SEL2 |= SPI_CLK;
    P1SEL2 |= SPI_SOMI;
    P1SEL2 |= SPI_SIMO;

    P1DIR |= SPI_CLK;
    P1DIR |= SPI_SOMI;
    P1DIR &= ~SPI_SIMO;

    UCB0CTL1 = UCSWRST;     // put state machine in reset
    UCB0CTL0 |= UCMSB;      // MSB first
    UCB0CTL0 |= UCSYNC;     // syncronous mode
    UCB0CTL1 &= UCSWRST;    // take out of reset

    IE2 |= UCB0RXIE;        // enable RX interrupt

    // FETS
    
    P2DIR |= RED_FET;
    P2DIR |= GRN_FET;
    P2DIR |= BLU_FET;

    fets_off();
    
    // TLC5941

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
    TA0CTL = TASSEL_2 | MC_1;

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
}

// SPI RX
interrupt (USCIAB0RX_VECTOR) spi_rx(void)
{
    unsigned char data = UCB0RXBUF;

    // loopback
    while (!(IFG2 & UCB0TXIFG));    // wait for TX buffer to be reading
    UCB0TXBUF = data;
}
