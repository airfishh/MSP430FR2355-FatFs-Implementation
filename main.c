#include <msp430fr2355.h>
#include "driverlib.h"
#include "FatFs/diskio.h"
#include "FatFs/ff.h"
#include "HAL/HAL_SDCard.h"
#include <stdio.h>
#include <string.h>

SDCardLib sdLib;
FIL file;
FRESULT res;
DIRS dir;

FATFS test_for_new; // to check if a new file can be written
FIL test_file;

int result = 1;

void Software_Trim();
void blink_error(int n);
void capture_debug_state(void);
int write_to_sd(const char[]);
int fat_init();

#define MCLK_FREQ_MHZ 8

int main(void)
{
    P1OUT  = 0x00;
    P1DIR  = 0x00;
    P1SEL0 = 0x00;
    P1SEL1 = 0x00;
    P1REN  = 0x00;

    WDTCTL = WDTPW + WDTHOLD;
    PM5CTL0 &= ~LOCKLPM5;

    // Clock configuration - 8MHz
    __bis_SR_register(SCG0);
    CSCTL3 |= SELREF__REFOCLK;
    CSCTL1 = DCOFTRIMEN_1 | DCOFTRIM0 | DCOFTRIM1 | DCORSEL_3;
    CSCTL2 = FLLD_0 + 243;
    __delay_cycles(3);
    __bic_SR_register(SCG0);
    Software_Trim();
    CSCTL4 = SELMS__DCOCLKDIV | SELA__REFOCLK;

    // LED setup
    P6DIR |= BIT6;
    P6OUT &= ~BIT6;

    __enable_interrupt();

    SDCardLib_init(&sdLib, &sdIntf_MSP430FR2355);
    __delay_cycles(800000);
    fat_init();     // Initialise SD card

    const char testing[] = "Kessel Runners! \r \n";
    write_to_sd(testing);

    f_close(&file);

        while(1)
    {
        P6OUT ^= BIT6;                      // Blink the red LED
        __delay_cycles(800000);
    }
}

int fat_init()
{
    signed errCode = -1;

    while (errCode != FR_OK)
    { //go until f_open returns FR_OK (function successful)
        errCode = f_mount(0, &sdLib.fatfs); //mount drive number 0
        errCode = f_opendir(&dir, "/"); //root directory

        errCode = f_open(&file, "TEST.txt", FA_CREATE_ALWAYS | FA_WRITE);
        if(errCode != FR_OK)
        result=0; //used as a debugging flag
    }
    return 0;
}

// Write input text to SD card. If function returns 1, success. If returns 0, failure
int write_to_sd(const char input_text[])
{
    UINT bytesWritten;
    res = f_write(&file, input_text, strlen(input_text), &bytesWritten);
    if (FR_OK == 0) 
    {
        return 1;
    }
    else {
        return 0;
    }
}

void Software_Trim()
{
    unsigned int oldDcoTap = 0xffff;
    unsigned int newDcoTap = 0xffff;
    unsigned int newDcoDelta = 0xffff;
    unsigned int bestDcoDelta = 0xffff;
    unsigned int csCtl0Copy = 0;
    unsigned int csCtl1Copy = 0;
    unsigned int csCtl0Read = 0;
    unsigned int csCtl1Read = 0;
    unsigned int dcoFreqTrim = 3;
    unsigned char endLoop = 0;

    do
    {
        CSCTL0 = 0x100;
        do
        {
            CSCTL7 &= ~DCOFFG;
        }while (CSCTL7 & DCOFFG);

        __delay_cycles((unsigned int)3000 * MCLK_FREQ_MHZ);
        while((CSCTL7 & (FLLUNLOCK0 | FLLUNLOCK1)) && ((CSCTL7 & DCOFFG) == 0));

        csCtl0Read = CSCTL0;
        csCtl1Read = CSCTL1;

        oldDcoTap = newDcoTap;
        newDcoTap = csCtl0Read & 0x01ff;
        dcoFreqTrim = (csCtl1Read & 0x0070)>>4;

        if(newDcoTap < 256)
        {
            newDcoDelta = 256 - newDcoTap;
            if((oldDcoTap != 0xffff) && (oldDcoTap >= 256))
                endLoop = 1;
            else
            {
                dcoFreqTrim--;
                CSCTL1 = (csCtl1Read & (~DCOFTRIM)) | (dcoFreqTrim<<4);
            }
        }
        else
        {
            newDcoDelta = newDcoTap - 256;
            if(oldDcoTap < 256)
                endLoop = 1;
            else
            {
                dcoFreqTrim++;
                CSCTL1 = (csCtl1Read & (~DCOFTRIM)) | (dcoFreqTrim<<4);
            }
        }

        if(newDcoDelta < bestDcoDelta)
        {
            csCtl0Copy = csCtl0Read;
            csCtl1Copy = csCtl1Read;
            bestDcoDelta = newDcoDelta;
        }

    }while(endLoop == 0);

    CSCTL0 = csCtl0Copy;
    CSCTL1 = csCtl1Copy;
    while(CSCTL7 & (FLLUNLOCK0 | FLLUNLOCK1));
}
