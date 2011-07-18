#include <msp430.h>
#include <legacymsp430.h>

#define LED_GRN       BIT1
#define LED_RED       BIT0
#define LED_PORT_OUT  P1OUT
#define LED_PORT_DIR  P1DIR

#define SPI_CLK       BIT3
#define SPI_DATA      BIT5
#define SPI_LATCH     BIT4
#define SPI_PORT_OUT  P4OUT
#define SPI_PORT_DIR  P4DIR

// array to hold the table data
unsigned char table[16*8*3];

// points to the current RX/TX position in the array
unsigned int rx_pointer = 0;
unsigned int tx_pointer = 0;

unsigned char rx_byte = 0;
unsigned char tx_byte = 0;

int main(void)
{

  //
  // WTD
  //
  
  WDTCTL = WDTPW + WDTHOLD;   // stop WTD

  //
  // clock
  //

  BCSCTL1 = CALBC1_12MHZ;     // 12Mhz clock
  DCOCTL = CALDCO_12MHZ;      //

  //
  // status LEDs
  //

  LED_PORT_DIR |= LED_GRN;
  LED_PORT_DIR |= LED_RED;

  //
  // UART
  //
  // http://www.daycounter.com/Calculators/MSP430-Uart-Calculator.phtml
  //

  P3SEL |= (BIT4 | BIT5);                     // P3.4 P3.5 = TX / RX
  P3DIR |= BIT4;
  P3DIR &= ~BIT5;

  UCA0CTL1 |= UCSSEL_2;             // SMCLK
  UCA0BR0 = 0x18;                      // 1MHz 115200
  UCA0BR1 = 0x00;                      // 1MHz 115200
  UCA0MCTL = 0x00;       // Modulation UCBRSx = 5
  UCA0CTL1 &= ~UCSWRST;             // **Initialize USCI state machine**
  IE2 |= UCA0RXIE;                  // Enable USCI_A0 RX interrupt

  //
  // SPI
  //
  
  SPI_PORT_DIR |= SPI_CLK;
  SPI_PORT_DIR |= SPI_DATA;
  SPI_PORT_DIR |= SPI_LATCH;

  SPI_PORT_OUT &= ~SPI_CLK;
  SPI_PORT_OUT &= ~SPI_DATA;
  SPI_PORT_OUT &= ~SPI_LATCH;

  // green power LED
  LED_PORT_OUT |= LED_GRN;
  LED_PORT_OUT |= LED_RED;

  // zero out variables
  rx_pointer = 0;
  tx_pointer = 0;
  rx_byte = 0;

  // enable global interrupts
  _EINT();

  // continuously send table array
  while (1) 
  {
    // grab current byte
    tx_byte = table[tx_pointer];

    // increment the pointer
    tx_pointer++;

    // if we reached the end, reset the pointer
    if (tx_pointer == (8*16*3))
    {
      tx_pointer = 0;
      SPI_PORT_OUT ^= SPI_LATCH;
    }

    // send out the data, fully unrolled for speed
    if (tx_byte & (1 << 7)) { SPI_PORT_OUT |= SPI_DATA; } else { SPI_PORT_OUT &= ~SPI_DATA; }

    SPI_PORT_OUT |= SPI_CLK;
    SPI_PORT_OUT &= ~SPI_CLK;
    
    if (tx_byte & (1 << 6)) { SPI_PORT_OUT |= SPI_DATA; } else { SPI_PORT_OUT &= ~SPI_DATA; }

    SPI_PORT_OUT |= SPI_CLK;
    SPI_PORT_OUT &= ~SPI_CLK;
    if (tx_byte & (1 << 5)) { SPI_PORT_OUT |= SPI_DATA; } else { SPI_PORT_OUT &= ~SPI_DATA; }

    SPI_PORT_OUT |= SPI_CLK;
    SPI_PORT_OUT &= ~SPI_CLK;
    
    if (tx_byte & (1 << 4)) { SPI_PORT_OUT |= SPI_DATA; } else { SPI_PORT_OUT &= ~SPI_DATA; }

    SPI_PORT_OUT |= SPI_CLK;
    SPI_PORT_OUT &= ~SPI_CLK;

    if (tx_byte & (1 << 3)) { SPI_PORT_OUT |= SPI_DATA; } else { SPI_PORT_OUT &= ~SPI_DATA; }

    SPI_PORT_OUT |= SPI_CLK;
    SPI_PORT_OUT &= ~SPI_CLK;

    if (tx_byte & (1 << 2)) { SPI_PORT_OUT |= SPI_DATA; } else { SPI_PORT_OUT &= ~SPI_DATA; }

    SPI_PORT_OUT |= SPI_CLK;
    SPI_PORT_OUT &= ~SPI_CLK;

    if (tx_byte & (1 << 1)) { SPI_PORT_OUT |= SPI_DATA; } else { SPI_PORT_OUT &= ~SPI_DATA; }

    SPI_PORT_OUT |= SPI_CLK;
    SPI_PORT_OUT &= ~SPI_CLK;

    if (tx_byte & (1 << 0)) { SPI_PORT_OUT |= SPI_DATA; } else { SPI_PORT_OUT &= ~SPI_DATA; }

    SPI_PORT_OUT |= SPI_CLK;
    SPI_PORT_OUT &= ~SPI_CLK;
    
    SPI_PORT_OUT &= ~SPI_DATA;
  }

  return 0;

}

interrupt (USCIAB0RX_VECTOR) uart_rx(void)
{

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
    if (rx_pointer == (8*16*3))
    {
      rx_pointer = 0;
      LED_PORT_OUT |= LED_RED;
    }
  }

}
