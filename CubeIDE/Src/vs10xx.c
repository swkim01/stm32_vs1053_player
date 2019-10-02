/**    
 * |----------------------------------------------------------------------
 * | Copyright (C) Aquilegia, 2019
 * | 
 * | This program is free software: you can redistribute it and/or modify
 * | it under the terms of the GNU General Public License as published by
 * | the Free Software Foundation, either version 3 of the License, or
 * | any later version.
 * |  
 * | This program is distributed in the hope that it will be useful,
 * | but WITHOUT ANY WARRANTY; without even the implied warranty of
 * | MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * | GNU General Public License for more details.
 * | 
 * | You should have received a copy of the GNU General Public License
 * | along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * |----------------------------------------------------------------------
 */
/*
 * Functions for interfacing with the mp3 player chip.
 */

#include "main.h"
#include "vs10xx.h"

extern SPI_HandleTypeDef hspi1;

/** initialization of VS10xx */
void vs10xx_init(void)
{
    HAL_GPIO_WritePin(XCS_GPIO_Port, XCS_Pin, 1);
    HAL_GPIO_WritePin(XDCS_GPIO_Port, XDCS_Pin, 1);
    HAL_GPIO_WritePin(XRESET_GPIO_Port, XRESET_Pin, 1);
}

/** Write the 16-bit value to VS10xx register */
void vs10xx_write_command(unsigned char addressbyte, unsigned int value)
{
    uint8_t buffer[4] = {
        VS_WRITE_COMMAND,
        addressbyte,
        value >> 8,
        value & 0xFF
    };

    HAL_GPIO_WritePin(XCS_GPIO_Port, XCS_Pin, 1);
    while (!vs10xx_readyfordata());

    HAL_GPIO_WritePin(XCS_GPIO_Port, XCS_Pin, 0);
    HAL_SPI_Transmit(&hspi1, buffer, sizeof(buffer), 10);
    HAL_GPIO_WritePin(XCS_GPIO_Port, XCS_Pin, 1);

    while (!vs10xx_readyfordata());
}

/** Read the 16-bit value of a VS10xx register */
unsigned int vs10xx_read_command(unsigned char addressbyte)
{
    unsigned int resultvalue = 0;
    uint8_t txbuffer[2] = {
            VS_READ_COMMAND, addressbyte
    };
    uint8_t datain = 0, dataout = 0xFF;
    
    HAL_GPIO_WritePin(XCS_GPIO_Port, XCS_Pin, 1);
    while (!vs10xx_readyfordata());

    HAL_GPIO_WritePin(XCS_GPIO_Port, XCS_Pin, 0);
    HAL_SPI_Transmit(&hspi1, txbuffer, sizeof(txbuffer), 10);

    HAL_SPI_TransmitReceive(&hspi1, &dataout, &datain, 1, 10);
    resultvalue = datain << 8;
    HAL_SPI_TransmitReceive(&hspi1, &dataout, &datain, 1, 10);
    resultvalue |= datain; 
    HAL_GPIO_WritePin(XCS_GPIO_Port, XCS_Pin, 1);

    return resultvalue;
}

/** write data to VS10xx  */
void vs10xx_write_data(unsigned char *databuf, unsigned char n)
{
    uint8_t dataout = 0xFF;

    HAL_GPIO_WritePin(XDCS_GPIO_Port, XDCS_Pin, 0);
    while (n--)
    {
        HAL_SPI_Transmit(&hspi1, databuf++, 1, 10);
    }
    HAL_GPIO_WritePin(XDCS_GPIO_Port, XDCS_Pin, 1);
}

void vs10xx_set_frequency(int freq)
{
    uint8_t i;
    uint32_t apb_freq = HAL_RCC_GetPCLK2Freq();

    /* Calculate prescaler value */
    /* Bits 5:3 in CR1 SPI registers are prescalers */
    /* 000 = 2, 001 = 4, 002 = 8, ..., 111 = 256 */
    for (i = 0; i < 8; i++)
        if (apb_freq / (1 << (i + 1)) <= freq)
            break;
    hspi1.Init.BaudRatePrescaler = i << 3;
    if(HAL_SPI_Init(&hspi1) != HAL_OK) return;
}

void vs10xx_set_volume(unsigned char vol)
{
    vs10xx_write_command(SPI_VOL, vol*0x101);     //Set volume level
}

unsigned int vs10xx_readyfordata(void)
{
    return HAL_GPIO_ReadPin(DREQ_GPIO_Port, DREQ_Pin);
}

/** Soft Reset of VS10xx (Between songs) */
void vs10xx_softreset(void)
{
    /* Low Speed SPI Baudrate <= 12 / 7 = 1.75MHz */
    vs10xx_set_frequency(1000000);
    
    /* Soft Reset of VS10xx */
    vs10xx_write_command(SPI_MODE, SM_SDINEW | SM_RESET); /* Newmode, Reset, No L1-2 */
    HAL_Delay(100);         //delay
    
    while (!vs10xx_readyfordata());
    
    /* A quick sanity check: write to two registers, then test if we
     get the same results. Note that if you use a too high SPI
     speed, the MSB is the most likely to fail when read again. */
    vs10xx_write_command(SPI_HDAT0, 0xABAD);
    vs10xx_write_command(SPI_HDAT1, 0x1DEA);
    if (vs10xx_read_command(SPI_HDAT0) != 0xABAD || vs10xx_read_command(SPI_HDAT1) != 0x1DEA) {
        printf("There is something wrong with VS10xx\n");
    }
    
    //vs10xx_write_command(SPI_CLOCKF, 0XC000);  //Set the clock
    vs10xx_write_command(SPI_CLOCKF, 0X6000);  //Set the clock
    vs10xx_write_command(SPI_AUDATA, 0xbb81);  //samplerate 48k,stereo
    vs10xx_write_command(SPI_BASS, 0x0055);    //set accent
    vs10xx_write_command(SPI_VOL, 0x4040);     //Set volume level
        
    while (!vs10xx_readyfordata());
}

/** Reset VS10xx */
void vs10xx_reset(void)
{
    uint8_t dataout = 0xFF;

    HAL_GPIO_WritePin(XRESET_GPIO_Port, XRESET_Pin, 0);
    HAL_Delay(2);  //it is a must
    
    /* Send dummy SPI byte to initialize SPI */
    HAL_SPI_Transmit(&hspi1, &dataout, 1, 10);

    /* Un-reset VS10XX chip */
    HAL_GPIO_WritePin(XCS_GPIO_Port, XCS_Pin, 1);
    HAL_GPIO_WritePin(XDCS_GPIO_Port, XDCS_Pin, 1);
    HAL_GPIO_WritePin(XRESET_GPIO_Port, XRESET_Pin, 1);

    vs10xx_softreset(); //vs10xx soft reset.

}


/*     Loads a plugin.       */
void vs10xx_loadplugin(const unsigned short *plugin, int length)
{
    int i = 0;
    while (i<length) {
        unsigned short addr, n, val;
        addr = plugin[i++];
        n = plugin[i++];
        if (n & 0x8000U) { /* RLE run, replicate n samples */
            n &= 0x7FFF;
            val = plugin[i++];
            while (n--) {
                vs10xx_write_command(addr, val);
            }
        } else {           /* Copy run, copy n samples */
            while (n--) {
                val = plugin[i++];
                vs10xx_write_command(addr, val);
            }
        }
    }
}

