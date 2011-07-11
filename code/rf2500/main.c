//******************************************************************************
//  Demo Application for MSP430 eZ430-RF2500 / CC1100-2500 Interface
//  Main code application library v1.1
//
// W. Goh
// Version 1.1
// Texas Instruments, Inc
// December 2009
// Built with IAR Embedded Workbench Version: 4.20
//******************************************************************************
// Change Log:
//******************************************************************************
// Version:  1.1
// Comments: Main application code designed for eZ430-RF2500 board
// Version:  1.00
// Comments: Initial Release Version
//******************************************************************************
#include <msp430.h>
#include <legacymsp430.h>
#include "include.h"

extern char paTable[];
extern char paTableLen;

char txBuffer[4];
char rxBuffer[4];
unsigned int i = 0;

int main (void)
{
    WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT

    //BCSCTL1 = CALBC1_1MHZ;
    //DCOCTL = CALDCO_1MHZ;

    TI_CC_SPISetup();                         // Initialize SPI port

    P2SEL = 0;                                // Sets P2.6 & P2.7 as GPIO
    TI_CC_PowerupResetCCxxxx();               // Reset CCxxxx
    writeRFSettings();                        // Write RF settings to config reg
    TI_CC_SPIWriteBurstReg(TI_CCxxx0_PATABLE, paTable, paTableLen);//Write PATABLE

    // Configure ports -- switch inputs, LEDs, GDO0 to RX packet info from CCxxxx
    TI_CC_SW_PxREN = TI_CC_SW1;               // Enable Pull up resistor
    TI_CC_SW_PxOUT = TI_CC_SW1;               // Enable pull up resistor
    TI_CC_SW_PxIES = TI_CC_SW1;               // Int on falling edge
    TI_CC_SW_PxIFG &= ~(TI_CC_SW1);           // Clr flags
    TI_CC_SW_PxIE = TI_CC_SW1;                // Activate interrupt enables
    TI_CC_LED_PxOUT &= ~(TI_CC_LED1 + TI_CC_LED2); // Outputs = 0
    TI_CC_LED_PxDIR |= TI_CC_LED1 + TI_CC_LED2;// LED Direction to Outputs

    TI_CC_GDO0_PxIES |= TI_CC_GDO0_PIN;       // Int on falling edge (end of pkt)
    TI_CC_GDO0_PxIFG &= ~TI_CC_GDO0_PIN;      // Clear flag
    TI_CC_GDO0_PxIE |= TI_CC_GDO0_PIN;        // Enable int on end of packet

    TI_CC_SPIStrobe(TI_CCxxx0_SRX);           // Initialize CCxxxx in RX mode.
                                              // When a pkt is received, it will
                                              // signal on GDO0 and wake CPU

    //_EINT();

    //TI_CC_LED_PxOUT |= TI_CC_LED1;

    //__bis_SR_register(LPM3_bits + GIE);       // Enter LPM3, enable interrupts

    P4DIR |= BIT6;
    while (1) {
    
        P4OUT |= BIT6;
        P4OUT &= ~BIT6;
    
    }

    return 0;
}


// The ISR assumes the interrupt came from a pressed button
interrupt (PORT1_VECTOR) Port1_ISR (void)
{
    // If Switch was pressed
    if(TI_CC_SW_PxIFG & TI_CC_SW1)
    {
        // Build packet
        txBuffer[0] = 2;                        // Packet length
        txBuffer[1] = 0x01;                     // Packet address
        txBuffer[2] = (~TI_CC_SW_PxIFG << 1) & 0x02; // Load switch inputs
        RFSendPacket(txBuffer, 3);              // Send value over RF
        __delay_cycles(5000);                   // Switch debounce
    }
    TI_CC_SW_PxIFG &= ~(TI_CC_SW1);           // Clr flag that caused int
}

// The ISR assumes the interrupt came from GDO0. GDO0 fires indicating that
// CCxxxx received a packet
interrupt (PORT2_VECTOR) Port2_ISR(void)
{
    // if GDO fired
    if(TI_CC_GDO0_PxIFG & TI_CC_GDO0_PIN)
    {
        char len=2;                             // Len of pkt to be RXed (only addr
        // plus data; size byte not incl b/c
        // stripped away within RX function)
        if (RFReceivePacket(rxBuffer,&len))     // Fetch packet from CCxxxx
            TI_CC_LED_PxOUT ^= rxBuffer[1];         // Toggle LEDs according to pkt data
    }

    TI_CC_GDO0_PxIFG &= ~TI_CC_GDO0_PIN;      // After pkt RX, this flag is set.
}
