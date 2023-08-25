//
// Arduino work-alike wrapper functions
// written by Larry Bank
// bitbank@pobox.com
// Copyright (c) 2023 BitBank Software, Inc.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#include "debug.h"
#include "Arduino.h"

#ifdef BITBANG
uint8_t u8SDA_Pin, u8SCL_Pin;
int iDelay = 1;
#endif

void delay(int i)
{
	Delay_Ms(i);
}
// Arduino-like API defines and function wrappers for WCH MCUs

void pinMode(uint8_t u8Pin, int iMode)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    if (u8Pin < 0xa0 || u8Pin > 0xdf) return; // invalid pin number

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 << (u8Pin & 0xf);
    if (iMode == OUTPUT)
    	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    else if (iMode == INPUT)
    	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    else if (iMode == INPUT_PULLUP)
    	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    else if (iMode == INPUT_PULLDOWN)
    	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    switch (u8Pin & 0xf0) {
    case 0xa0:
    	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
        GPIO_Init(GPIOA, &GPIO_InitStructure);
    	break;
    case 0xc0:
    	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
        GPIO_Init(GPIOC, &GPIO_InitStructure);
    	break;
    case 0xd0:
    	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);
        GPIO_Init(GPIOD, &GPIO_InitStructure);
    	break;
    }
} /* pinMode() */

uint8_t digitalRead(uint8_t u8Pin)
{
    uint8_t u8GPIO = GPIO_Pin_0 << (u8Pin & 0xf);
    uint8_t u8Value = 0;
    switch (u8Pin & 0xf0) {
    case 0xa0:
    	u8Value = GPIO_ReadInputDataBit(GPIOA, u8GPIO);
    	break;
    case 0xc0:
    	u8Value = GPIO_ReadInputDataBit(GPIOC, u8GPIO);
    	break;
    case 0xd0:
    	u8Value = GPIO_ReadInputDataBit(GPIOD, u8GPIO);
    	break;
    }
    return u8Value;
} /* digitalRead() */

void digitalWrite(uint8_t u8Pin, uint8_t u8Value)
{
	uint8_t u8GPIO = GPIO_Pin_0 << (u8Pin & 0xf);
	u8Value = (u8Value) ? Bit_SET : Bit_RESET;

	switch (u8Pin & 0xf0) {
	case 0xa0:
		GPIO_WriteBit(GPIOA, u8GPIO, u8Value);
		break;
	case 0xc0:
		GPIO_WriteBit(GPIOC, u8GPIO, u8Value);
		break;
	case 0xd0:
		GPIO_WriteBit(GPIOD, u8GPIO, u8Value);
		break;
	}
} /* digitalWrite() */

#ifdef BITBANG
uint8_t SDA_READ(void)
{
	return digitalRead(u8SDA_Pin);
}
void SDA_HIGH(void)
{
	pinMode(u8SDA_Pin, INPUT);
}
void SDA_LOW(void)
{
	pinMode(u8SDA_Pin, OUTPUT);
	digitalWrite(u8SDA_Pin, 0);
}
void SCL_HIGH(void)
{
	pinMode(u8SCL_Pin, INPUT);
}
void SCL_LOW(void)
{
	pinMode(u8SCL_Pin, OUTPUT);
	digitalWrite(u8SCL_Pin, 0);
}
void I2CSetSpeed(int iSpeed)
{
	if (iSpeed >= 400000) iDelay = 1;
	else if (iSpeed >= 100000) iDelay = 10;
	else iDelay = 20;
}
void I2CInit(uint8_t u8SDA, uint8_t u8SCL, int iSpeed)
{
	u8SDA_Pin = u8SDA;
	u8SCL_Pin = u8SCL;
	if (iSpeed >= 400000) iDelay = 1;
	else if (iSpeed >= 100000) iDelay = 10;
	else iDelay = 20;
} /* I2CInit() */

void my_sleep_us(int iDelay)
{
	Delay_Us(iDelay);
}
// Transmit a byte and read the ack bit
// if we get a NACK (negative acknowledge) return 0
// otherwise return 1 for success
//

int i2cByteOut(uint8_t b)
{
uint8_t i, ack;

for (i=0; i<8; i++)
{
//    my_sleep_us(iDelay);
    if (b & 0x80)
      SDA_HIGH(); // set data line to 1
    else
      SDA_LOW(); // set data line to 0
    b <<= 1;
//    my_sleep_us(iDelay);
    SCL_HIGH(); // clock high (slave latches data)
    my_sleep_us(iDelay);
    SCL_LOW(); // clock low
    my_sleep_us(iDelay);
} // for i
//my_sleep_us(iDelay);
// read ack bit
SDA_HIGH(); // set data line for reading
//my_sleep_us(iDelay);
SCL_HIGH(); // clock line high
my_sleep_us(iDelay); // DEBUG - delay/2
ack = SDA_READ();
//my_sleep_us(iDelay);
SCL_LOW(); // clock low
my_sleep_us(iDelay); // DEBUG - delay/2
SDA_LOW(); // data low
return (ack == 0); // a low ACK bit means success
} /* i2cByteOut() */

//
// Receive a byte and read the ack bit
// if we get a NACK (negative acknowledge) return 0
// otherwise return 1 for success
//
uint8_t i2cByteIn(uint8_t bLast)
{
uint8_t i;
uint8_t b = 0;

     SDA_HIGH(); // set data line as input
     for (i=0; i<8; i++)
     {
         my_sleep_us(iDelay); // wait for data to settle
         SCL_HIGH(); // clock high (slave latches data)
         my_sleep_us(iDelay);
         b <<= 1;
         if (SDA_READ() != 0) // read the data bit
           b |= 1; // set data bit
         SCL_LOW(); // clock low
     } // for i
     if (bLast)
        SDA_HIGH(); // last byte sends a NACK
     else
        SDA_LOW();
//     my_sleep_us(iDelay);
     SCL_HIGH(); // clock high
     my_sleep_us(iDelay);
     SCL_LOW(); // clock low to send ack
     my_sleep_us(iDelay);
//     SDA_HIGH();
     SDA_LOW(); // data low
  return b;
} /* i2cByteIn() */
//
// Send I2C STOP condition
//
void i2cEnd(void)
{
   SDA_LOW(); // data line low
   my_sleep_us(iDelay);
   SCL_HIGH(); // clock high
   my_sleep_us(iDelay);
   SDA_HIGH(); // data high
   my_sleep_us(iDelay);
} /* i2cEnd() */

int i2cBegin(uint8_t addr, uint8_t bRead)
{
   int rc;
//   SCL_HIGH();
//   my_sleep_us(iDelay);
   SDA_LOW(); // data line low first
   my_sleep_us(iDelay);
   SCL_LOW(); // then clock line low is a START signal
   addr <<= 1;
   if (bRead)
      addr++; // set read bit
   rc = i2cByteOut(addr); // send the slave address and R/W bit
   return rc;
} /* i2cBegin() */

void I2CWrite(uint8_t addr, uint8_t *pData, int iLen)
{
uint8_t b;
int rc;

   i2cBegin(addr, 0);
   rc = 1;
   while (iLen && rc == 1)
   {
      b = *pData++;
      rc = i2cByteOut(b);
      if (rc == 1) // success
      {
         iLen--;
      }
   } // for each byte
   i2cEnd();
//return (rc == 1) ? (iOldLen - iLen) : 0; // 0 indicates bad ack from sending a byte
} /* I2CWrite() */

void I2CRead(uint8_t addr, uint8_t *pData, int iLen)
{
   i2cBegin(addr, 1);
   while (iLen--)
   {
      *pData++ = i2cByteIn(iLen == 0);
   } // for each byte
   i2cEnd();
} /* I2CRead() */

int I2CTest(uint8_t addr)
{
int response = 0;

   if (i2cBegin(addr, 0)) // try to write to the given address
   {
      response = 1;
   }
   i2cEnd();
return response;
} /* I2CTest() */

#else // hardware I2C

void I2CSetSpeed(int iSpeed)
{
    I2C_InitTypeDef I2C_InitTSturcture={0};

    I2C_InitTSturcture.I2C_ClockSpeed = iSpeed;
    I2C_InitTSturcture.I2C_Mode = I2C_Mode_I2C;
    I2C_InitTSturcture.I2C_DutyCycle = I2C_DutyCycle_16_9;
    I2C_InitTSturcture.I2C_OwnAddress1 = 0x02; //address; sender's unimportant address
    I2C_InitTSturcture.I2C_Ack = I2C_Ack_Enable;
    I2C_InitTSturcture.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_Init( I2C1, &I2C_InitTSturcture );
} /* I2CSetSpeed() */

void I2CInit(uint8_t iSDA, uint8_t iSCL, int iSpeed)
{
	(void)iSDA; (void)iSCL;

    GPIO_InitTypeDef GPIO_InitStructure={0};

    // Fixed to pins C1/C2 for now
    RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO, ENABLE );
    RCC_APB1PeriphClockCmd( RCC_APB1Periph_I2C1, ENABLE );

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init( GPIOC, &GPIO_InitStructure );

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init( GPIOC, &GPIO_InitStructure );

    I2C_DeInit(I2C1);
    I2CSetSpeed(iSpeed);

    I2C_Cmd( I2C1, ENABLE );

    I2C_AcknowledgeConfig( I2C1, ENABLE );
    while( I2C_GetFlagStatus( I2C1, I2C_FLAG_BUSY ) != RESET );
} /* I2CInit() */

//
// Returns 0 for timeout error
// returns 1 for success
//
int I2CRead(uint8_t u8Addr, uint8_t *pData, int iLen)
{
	int iTimeout = 0;

    I2C_GenerateSTART( I2C1, ENABLE );
    while( !I2C_CheckEvent( I2C1, I2C_EVENT_MASTER_MODE_SELECT ) );

    I2C_Send7bitAddress( I2C1, u8Addr<<1, I2C_Direction_Receiver );

    while(iTimeout < 10000 && !I2C_CheckEvent( I2C1, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED ) ) {
    	iTimeout++;
    }
    if (iTimeout >= 10000) return 0; // error

    iTimeout = 0;
    while(iLen && iTimeout < 10000)
    {
        if( I2C_GetFlagStatus( I2C1, I2C_FLAG_RXNE ) !=  RESET )
        {
        	iTimeout = 0;
            pData[0] = I2C_ReceiveData( I2C1 );
            pData++;
            iLen--;
        } else {
        	iTimeout++;
        }
    }

    I2C_GenerateSTOP( I2C1, ENABLE );
    return (iLen == 0);

} /* I2CRead() */

void I2CWrite(uint8_t u8Addr, uint8_t *pData, int iLen)
{
    I2C_GenerateSTART( I2C1, ENABLE );
    while( !I2C_CheckEvent( I2C1, I2C_EVENT_MASTER_MODE_SELECT ) );

    I2C_Send7bitAddress( I2C1, u8Addr<<1, I2C_Direction_Transmitter );

    while( !I2C_CheckEvent( I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED ) );

    while(iLen)
    {
        if( I2C_GetFlagStatus( I2C1, I2C_FLAG_TXE ) !=  RESET )
        {
            I2C_SendData( I2C1, pData[0] );
            pData++;
            iLen--;
        }
    }

    while( !I2C_CheckEvent( I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED ) );
    I2C_GenerateSTOP( I2C1, ENABLE );

} /* I2CWrite() */

int I2CTest(uint8_t u8Addr)
{
	int iTimeout = 0;

	I2C_ClearFlag(I2C1, I2C_FLAG_AF);
    I2C_GenerateSTART( I2C1, ENABLE );
    while( !I2C_CheckEvent( I2C1, I2C_EVENT_MASTER_MODE_SELECT ) );

    I2C_Send7bitAddress( I2C1, u8Addr<<1, I2C_Direction_Transmitter );

    while(iTimeout < 10000 && !I2C_CheckEvent( I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED ) ) {
    	iTimeout++;
    }
    if (iTimeout >= 10000) return 0; // no device at that address; the MTMS flag will never get set

    I2C_GenerateSTOP( I2C1, ENABLE );
    // check ACK failure flag
    return (I2C_GetFlagStatus(I2C1, /*I2C_FLAG_TXE*/I2C_FLAG_AF) == RESET); // 0 = fail, 1 = succeed

} /* I2CTest() */
#endif // !BITBANG

//
// Read N bytes starting at a specific I2C internal register
// returns 1 for success, 0 for error
//
void I2CReadRegister(uint8_t iAddr, uint8_t u8Register, uint8_t *pData, int iLen)
{
  I2CWrite(iAddr, &u8Register, 1);
  I2CRead(iAddr, pData, iLen);
} /* I2CReadRegister() */

// Put CPU into standby mode for a multiple of 82ms tick increments
// max ticks value is 63
void Standby82ms(uint8_t iTicks)
{
    EXTI_InitTypeDef EXTI_InitStructure = {0};
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    // init external interrupts
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

    EXTI_InitStructure.EXTI_Line = EXTI_Line9;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Event;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    // Init GPIOs
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;

    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    // init wake up timer and enter standby mode
    RCC_LSICmd(ENABLE);
    while(RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET);
    PWR_AWU_SetPrescaler(PWR_AWU_Prescaler_10240);
    PWR_AWU_SetWindowValue(iTicks);
    PWR_AutoWakeUpCmd(ENABLE);
    PWR_EnterSTANDBYMode(PWR_STANDBYEntry_WFE);

    GPIO_DeInit(GPIOA);
    GPIO_DeInit(GPIOC);
    GPIO_DeInit(GPIOD);

} /* Standby82ms() */

void SPI_begin(int iSpeed, int iMode)
{
    GPIO_InitTypeDef GPIO_InitStructure={0};
    SPI_InitTypeDef SPI_InitStructure={0};

    RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOC | RCC_APB2Periph_SPI1, ENABLE );

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init( GPIOC, &GPIO_InitStructure );

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init( GPIOC, &GPIO_InitStructure );

    SPI_InitStructure.SPI_Direction = SPI_Direction_1Line_Tx;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;

    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = (iMode & 2) ? SPI_CPOL_High : SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = (iMode & 1) ? SPI_CPHA_2Edge : SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    if (iSpeed >= (SystemCoreClock/2))
    	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;
    else if (iSpeed >= (SystemCoreClock/4))
    	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;
    else if (iSpeed >= (SystemCoreClock/8))
    	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;
    else if (iSpeed >= (SystemCoreClock/16))
    	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_16;
    else if (iSpeed >= (SystemCoreClock/32))
    	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_32;
    else if (iSpeed >= (SystemCoreClock/64))
    	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_64;
    else if (iSpeed >= (SystemCoreClock/128))
    	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_128;
    else
    	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_256;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init( SPI1, &SPI_InitStructure );

    SPI_I2S_DMACmd(SPI1, SPI_I2S_DMAReq_Tx, ENABLE); // enable DMA on transmit

    SPI_Cmd( SPI1, ENABLE );

} /* SPI_begin() */

// polling write
void SPI_write(uint8_t *pData, int iLen)
{
	int i = 0;

    while (i < iLen)
    {
    	if ( SPI_I2S_GetFlagStatus( SPI1, SPI_I2S_FLAG_TXE ) != RESET )
          SPI_I2S_SendData( SPI1, pData[i++] );
    }
    // wait until transmit empty flag is true
    while (SPI_I2S_GetFlagStatus( SPI1, SPI_I2S_FLAG_TXE ) == RESET)
    {};
    while (SPI_I2S_GetFlagStatus( SPI1, SPI_I2S_FLAG_BSY ) == SET)
    {}; // wait until it's not busy

} /* SPI_write() */
