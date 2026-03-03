#include "HAL_SDCard.h"
#include "driverlib.h"

#define CLOCK_FREQ              8000000
#define DESIRED_SPI_FREQ        400000
#define DESIRED_SPI_FAST_FREQ   4000000

SDCardLib_Interface sdIntf_MSP430FR2355 = {
    CLOCK_FREQ,
    SDCard_init,
    SDCard_fastMode,
    SDCard_readFrame,
    SDCard_sendFrame,
    SDCard_setCSHigh,
    SDCard_setCSLow,
    SDCard_detectCard,
};

void SDCard_init(void)
{
    UCA0CTLW0 |= UCSWRST;    // UCSWRST = 1 for register init

    // Init registers
    UCA0CTLW0 = UCSWRST
                | UCCKPH
                | UCMSB
                | UCMST
                | UCMODE_0
                | UCSYNC
                | UCSSEL__SMCLK;

    // Clock divider set
    UCA0BRW = 20;                 // 8MHz / 20 = 400kHz

    // Clear modulation - required for SPI mode per datasheet
    UCA0MCTLW = 0;

    // Configure ports
    // UCA0 on FR2355: P1.5=CLK, P1.6=SOMI, P1.7=SIMO
    // Set to secondary module function (eUSCI_A)
    P1SEL0 |=  (BIT5 | BIT6 | BIT7);
    P1SEL1 &= ~(BIT5 | BIT6 | BIT7);

    // CS pin 4.0
    P4SEL0 &= ~BIT0;
    P4SEL1 &= ~BIT0;
    P4DIR  |=  BIT0;
    P4OUT  |=  BIT0;

    // UCSWRST to 0 for operation
    UCA0CTLW0 &= ~UCSWRST;
}

void SDCard_fastMode(void)
{
    UCA0CTLW0 |=  UCSWRST;
    UCA0BRW    =  2;            // 8MHz / 2 = 4MHz
    UCA0CTLW0 &= ~UCSWRST;
}

void SDCard_readFrame(uint8_t *pBuffer, uint16_t size)
{
    uint16_t gie = __get_SR_register() & GIE;
    __disable_interrupt();

    UCA0IFG &= ~UCRXIFG;

    while(size--)
    {
        while(!(UCA0IFG & UCTXIFG));
        UCA0TXBUF = 0xFF;
        while(!(UCA0IFG & UCRXIFG));
        *pBuffer++ = UCA0RXBUF;
    }

    __bis_SR_register(gie);
}

void SDCard_sendFrame(uint8_t *pBuffer, uint16_t size)
{
    uint16_t gie = __get_SR_register() & GIE;
    __disable_interrupt();

    while(size--)
    {
        while(!(UCA0IFG & UCTXIFG));
        UCA0TXBUF = *pBuffer++;
    }

    // Wait for transmission complete
    while(UCA0STATW & UCBUSY);

    // Dummy read to clear overrun
    volatile uint8_t dummy = UCA0RXBUF;

    __bis_SR_register(gie);
}

void SDCard_setCSHigh(void)
{
    P4OUT |= BIT0;
}

void SDCard_setCSLow(void)
{
    P4OUT &= ~BIT0;
}

SDCardLib_Status SDCard_detectCard(void)
{
    return SDCARDLIB_STATUS_PRESENT;
}
