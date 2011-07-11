//----------------------------------------------------------------------------
//  Description:  This file contains definitions specific to the hardware board.
//  Specifically, the definitions include hardware connections with the
//  CCxxxx connector port, LEDs, and switches.
//
//  MSP430/CC1100-2500 Interface Code Library v1.0
//
//  K. Quiring
//  Texas Instruments, Inc.
//  July 2006
//  IAR Embedded Workbench v3.41
//----------------------------------------------------------------------------

#define TI_CC_LED_PxOUT         P1OUT
#define TI_CC_LED_PxDIR         P1DIR
#define TI_CC_LED1              0x01
#define TI_CC_LED2              0x02
#define TI_CC_LED3              0x04
#define TI_CC_LED4              0x08

#define TI_CC_SW_PxIN           P1IN
#define TI_CC_SW_PxIE           P1IE
#define TI_CC_SW_PxIES          P1IES
#define TI_CC_SW_PxIFG          P1IFG
#define TI_CC_SW_PxREN          P1REN
#define TI_CC_SW_PxOUT          P1OUT
#define TI_CC_SW1               BIT2
#define TI_CC_SW2               BIT3
#define TI_CC_SW3               0x40
#define TI_CC_SW4               0x80

#define TI_CC_GDO0_PxOUT        P1OUT
#define TI_CC_GDO0_PxIN         P1IN
#define TI_CC_GDO0_PxDIR        P1DIR
#define TI_CC_GDO0_PxIE         P1IE
#define TI_CC_GDO0_PxIES        P1IES
#define TI_CC_GDO0_PxIFG        P1IFG
#define TI_CC_GDO0_PIN          0x10

#define TI_CC_GDO1_PxOUT        P5OUT
#define TI_CC_GDO1_PxIN         P5IN
#define TI_CC_GDO1_PxDIR        P5DIR
#define TI_CC_GDO1_PIN          0x04

#define TI_CC_GDO2_PxOUT        P2OUT
#define TI_CC_GDO2_PxIN         P2IN
#define TI_CC_GDO2_PxDIR        P2DIR
#define TI_CC_GDO2_PIN          0x40

#define TI_CC_CSn_PxOUT         P2OUT
#define TI_CC_CSn_PxDIR         P2DIR
#define TI_CC_CSn_PIN           BIT7


//----------------------------------------------------------------------------
// Select which port will be used for interface to CCxxxx
//----------------------------------------------------------------------------
#define TI_CC_RF_SER_INTF       TI_CC_SER_INTF_BITBANG  // Interface to CCxxxx

//#define TI_CC_RF_SER_INTF       TI_CC_SER_INTF_USCIB0  // Interface to CCxxxx