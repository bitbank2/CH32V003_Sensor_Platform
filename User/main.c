//
// Portable Sensor Platform
// by Larry Bank
//
// A CH32V003 project to allow easy field-testing of various I2C sensors
// The FLASH memory (16K) doesn't allow too many sensors + display routines to be included
// but the ones supported are auto-detected and use a nice large font to display the information
//

#include "debug.h"
#include "Arduino.h"
#include "sharp_lcd.h"
#include "ltr390.h"
#include "Roboto_Black_40.h"
#include "scd41.h"
#include "rtc_eeprom.h"
#include "LSM6DS3.h"

struct tm myTime;

// Hardware connections. The hex value represents the port (upper nibble) and GPIO pin (lower nibble)
// QFN20 PCB
//#define LCD_CS 0xd3
//#define LCD_VCOM 0xd4
// TSSOP20 PCB
#define LCD_CS 0xc7
#define LCD_VCOM 0xd3

#define LED_PIN 0xd6
#define SDA_PIN 0xc1
#define SCL_PIN 0xc2
#define BUTTON0_PIN 0xc3
#define BUTTON1_PIN 0xd5

int iSensor;
uint8_t bInvert = 0; // invert the LCD colors
extern uint32_t _iUV;

const char *szSensorNames[] = {"Unknown", "LTR390", "SCD4x", "LSM6DS3", "RV3032", "DS3231"};

enum {
	SENSOR_UNKNOWN,
	SENSOR_LTR390,
	SENSOR_SCD4X,
	SENSOR_LSM6DS3,
	SENSOR_RV3032,
	SENSOR_DS3231,
	SENSOR_COUNT
};

int GetButtons(void)
{
        int i = 0;
//      pinMode(BUTTON0_PIN, INPUT_PULLUP); // re-enable gpio in case it got disabled by standby mode
//      pinMode(BUTTON1_PIN, INPUT_PULLUP);
        if (digitalRead(BUTTON0_PIN) == 0) i|=1;
        if (digitalRead(BUTTON1_PIN) == 0) i|=2;
        return i;

} /* GetButtons() */

// Convert a number into a zero-terminated string
int i2str(char *pDest, int iVal)
{
        char *d = pDest;
        int i, iPlaceVal = 10000;
        int iDigits = 0;

        if (iVal < 0) {
                iDigits++;
                *d++ = '-';
                iVal = -iVal;
        }
        while (iPlaceVal) {
                if (iVal >= iPlaceVal) {
                        i = iVal / iPlaceVal;
                        *d++ = '0' + (char)i;
                        iVal -= (i*iPlaceVal);
                        iDigits++;
                } else if (iDigits != 0) {
                        *d++ = '0'; // non-zeros were already displayed
                }
                iPlaceVal /= 10;
        }
        if (d == pDest) // must be zero
                *d++ = '0';
        *d++ = 0; // terminator
        return (int)(d - pDest - 1); // string length
} /* i2str() */

// Convert a number into a zero-terminated string
void i2strf(char *pDest, int iVal, int iDigits)
{
        char *d = pDest;
        int i;
        pDest[iDigits] = 0;
        while (iDigits) {
        	iDigits--;
            i = iVal % 10;
            d[iDigits] = '0' + (char)i;
            iVal /= 10;
        }
} /* i2strf() */

void SetTime(void)
{
	int i, iFlash, iCount = 100, iTick = 0, iCursor = 0, bDone = 0;
	int iButts, iOldButts = 0;
	char szTemp[16];
	struct tm myTime;

	while (!bDone) {

		rtcGetTime(&myTime);
		iFlash = ((iTick & 15) > 3);
		sharpFill(bInvert);
		sharpWriteString(128,2,"Next", FONT_8x8, bInvert);
		sharpWriteString(152,58, "+", FONT_8x8, bInvert);
		sharpWriteString(16,2, "Set Time", FONT_12x16, bInvert);
	    i2strf(szTemp, myTime.tm_mday, 2);
	    if (iCursor != 0 || (iCursor == 0 && iFlash))
           sharpWriteString(0, 18, szTemp, FONT_12x16, bInvert);
	    sharpWriteString(24, 18, "/", FONT_12x16, bInvert);

	    i = myTime.tm_mon+1;
	    i2strf(szTemp, i, 2);
	    if (iCursor != 1 || (iCursor == 1 && iFlash))
           sharpWriteString(36, 18, szTemp, FONT_12x16, bInvert);
	    sharpWriteString(60, 18, "/", FONT_12x16, bInvert);

	    i = myTime.tm_year % 100;
	    i2strf(szTemp, i+2000, 2);
	    if (iCursor != 2 || (iCursor == 2 && iFlash))
           sharpWriteString(72, 18, szTemp, FONT_12x16, bInvert);

	    i2strf(szTemp, myTime.tm_hour, 2);
	    szTemp[2] = ':';
	    szTemp[3] = 0;
	    if (iCursor != 3 || (iCursor == 3 && iFlash))
           sharpWriteString(2, 34, szTemp, FONT_12x16, bInvert);
	    i2strf(szTemp, myTime.tm_min, 2);
	    szTemp[2] = ':';
	    szTemp[3] = 0;
	    if (iCursor != 4 || (iCursor == 4 && iFlash))
           sharpWriteString(38, 34, szTemp, FONT_12x16, bInvert);
	    i2strf(szTemp, myTime.tm_sec, 2);
	    if (iCursor != 5 || (iCursor == 5 && iFlash))
           sharpWriteString(74, 34, szTemp, FONT_12x16, bInvert);
	    iButts = GetButtons();
	    if (iButts == 3) { // user wants to exit without setting the time
	    	bDone = 1;
	    }
	    if (iCursor != 6 || (iCursor == 6 && iFlash))
	    	sharpWriteString(56, 50, "Done", FONT_12x16, bInvert);
	    if (iOldButts == 0 && iButts == 1) { // advance cursor
	    	iCursor++;
	    	if (iCursor > 6) iCursor = 0;
	    }
	    if (iOldButts == 0) iCount = 0; // reset "button held" counter
	    if (iButts == 2) iCount++;
	    if (iCount == 1 || iCount > 24) { // adjust value
	    	switch (iCursor) {
	    	case 0: // day of the month
	    		myTime.tm_mday++;
	    		if (myTime.tm_mday > 31) myTime.tm_mday = 0;
	    		break;
	    	case 1: // month
	    		myTime.tm_mon++;
	    		if (myTime.tm_mon > 11) myTime.tm_mon = 0;
	    		break;
	    	case 2: // year
	    		myTime.tm_year++;
	    		myTime.tm_year = (myTime.tm_year % 100) + 100;
	    		break;
	    	case 3: // hour
	    		myTime.tm_hour++;
	    		if (myTime.tm_hour > 23) myTime.tm_hour = 0;
	    		break;
	    	case 4: // minute
	    		myTime.tm_min++;
	    		if (myTime.tm_min > 59) myTime.tm_min = 0;
	    		break;
	    	case 5: // second
	    		myTime.tm_sec++;
	    		if (myTime.tm_sec > 59) myTime.tm_sec = 0;
	    		break;
	    	case 6:
	    		// set the time and leave
	    		rtcSetTime(&myTime);
	    		sharpFill(bInvert);
	            sharpWriteBuffer();
	    		return;
	    	}
	    }
	    iOldButts = iButts;
        sharpWriteBuffer();
        Delay_Ms(33);
        iTick++;
	} // while !bDone
} /* SetTime() */

void ShowTime(void)
{
char szTemp[16];

	sharpFill(bInvert);
	sharpWriteString(136, 2, "Set", FONT_8x8, bInvert);
	sharpWriteString(112, 58, "Invert", FONT_8x8, bInvert);
	rtcGetTime(&myTime);
   	i2strf(szTemp, myTime.tm_hour, 2);
   	szTemp[2] = ':';
   	i2strf(&szTemp[3], myTime.tm_min, 2);
   	sharpWriteStringCustom(&Roboto_Black_40, 2, 36, szTemp, !bInvert, 1);
   	i2strf(szTemp, myTime.tm_sec, 2);
   	sharpWriteString(112, 18, szTemp, FONT_12x16, bInvert);
   	i2strf(szTemp, myTime.tm_mday, 2);
   	szTemp[2] = '/';
   	i2strf(&szTemp[3], myTime.tm_mon+1, 2);
   	szTemp[5] = '/';
   	i2strf(&szTemp[6], myTime.tm_year + 1900, 4);
   	sharpWriteString(2, 42, szTemp, FONT_12x16, bInvert);
   	sharpWriteBuffer();
} /* ShowTime() */

void ShowLTR390Sample(int iValue, int iMax)
{
int iUVI;
char szTemp[16];
int i;

    sharpFill(bInvert);
	sharpWriteString(24, 6, "UVI   Max", FONT_12x16, bInvert);
	iUVI = ltr390_getUVI(iValue); // instaneous value
	i2str(szTemp, iUVI/10); // whole part
    sharpWriteStringCustom(&Roboto_Black_40, 0, 62, szTemp, !bInvert, 0);
    sharpWriteStringCustom(&Roboto_Black_40, -1, 62, ".", !bInvert, 0);
	i = iUVI % 10; // 10ths
	i2str(szTemp, i);
    sharpWriteStringCustom(&Roboto_Black_40, -1, 62, szTemp, !bInvert, 0);

	iUVI = ltr390_getUVI(iMax); // max value from the last 3.2 seconds
	i2str(szTemp, iUVI/10); // whole part
    sharpWriteStringCustom(&Roboto_Black_40, 84, 62, szTemp, !bInvert, 0);
    sharpWriteStringCustom(&Roboto_Black_40, -1, 62, ".", !bInvert, 0);
	i = iUVI % 10; // 10ths
	i2str(szTemp, i);
    sharpWriteStringCustom(&Roboto_Black_40, -1, 62, szTemp, !bInvert, 0);
// DEBUG
//    i2str(szTemp, iValue);
//	sharpWriteString(2, 22, szTemp, FONT_8x8, 0);

	sharpWriteBuffer();
} /* ShowLTR390Sample() */

void i2hex(char *pStr, int i)
{
	char c;
	c = (i >> 4);
	if (c > 9) c += 55;
	else c += 48;
	pStr[0] = c;
	c = i & 0xf;
	if (c > 9)  c += 55;
	else c += 48;
	pStr[1] = c;
	pStr[2] = 0;
} /* i2hex() */

int GetSensorType(int i, char *szName)
{
uint8_t cTemp[4];
int iType = SENSOR_UNKNOWN;

	if (i == 0x53) // could be Lite-On LTR390 UV light sensor
	{
		I2CReadRegister(i, 0x06, cTemp, 1); // Part ID
		if (cTemp[0] == 0xb2) // a match!
			iType = SENSOR_LTR390;
	}
    if (i == 0x62 && iType == SENSOR_UNKNOWN) {
	    // DEBUG - for now, assume it's the SCD4x
	   iType = SENSOR_SCD4X;
    }
    if (i == 0x68 && iType == SENSOR_UNKNOWN) { // look for DS3231
        // Make sure it's really a DS3231 because other I2C devices
        // use the same address (0x68)
         I2CReadRegister(i, 0x12, cTemp, 1); // read temp reg
         if ((cTemp[0] & 0x3f) == 0) {
            iType = SENSOR_DS3231;
         }
    }
    if (iType == SENSOR_UNKNOWN && i == 0x51) {
       // The PCF8563 and RV3032 use the same I2C address (0x51)
       // Try to write to the temperature threshold register to see
       // which one is connected
       cTemp[0] = 0x17; // temp threshold high register
       cTemp[1] = 0x55; // random value to write
       I2CWrite(i, cTemp, 2);
       I2CReadRegister(i, 0x17, cTemp, 1);
       if (cTemp[0] == 0x55) {
          iType = SENSOR_RV3032;
       } // else it must be the PCF8563
    }
    if (iType == SENSOR_UNKNOWN) {
    // Check for LSM6DS3
    	I2CReadRegister(i, 0x0f, cTemp, 1); // WHO_AM_I
    	if (cTemp[0] == 0x69) {
    		iType = SENSOR_LSM6DS3;
    	}
    }

    strcpy(szName, szSensorNames[iType]);
	return iType;
} /* GetSensorType() */

void ScanBus(void)
{
uint8_t i, y;
int iBad = 0;
char szTemp[16];

I2CInit(SDA_PIN, SCL_PIN, 100000);
scan_again:
	sharpFill(0);
    sharpWriteString(4, 12, "I2C Bus Scan", FONT_12x16, 0);
	sharpWriteBuffer();
	while (GetButtons() != 0) {
		Delay_Ms(100);
	}
	y = 28;
	for (i=4; i<128 && iSensor == SENSOR_UNKNOWN && iBad < 10; i++) {
		digitalWrite(LED_PIN, i & 1);
	    sharpWriteString(2, y, "0x", FONT_8x8, 0);
	    i2hex(szTemp, i);
	    sharpWriteString(-1, -1, szTemp, FONT_8x8, 0);
		sharpWriteBuffer();
		if (I2CTest(i)) {
			if (i < 16) {
				iBad++; // there shouldn't be anything with an address of less than 16
				continue;
			} // bad address
		    sharpWriteString(2, y, "0x", FONT_8x8, 0);
		    i2hex(szTemp, i);
		    sharpWriteString(-1, -1, szTemp, FONT_8x8, 0);
		    sharpWriteString(-1, -1, " --> ", FONT_8x8, 0);
		    iSensor = GetSensorType(i, szTemp);
		    sharpWriteString(-1, -1, szTemp, FONT_8x8, 0);
			sharpWriteBuffer();
			y+= 8;
		}
	}
	if (iSensor == SENSOR_UNKNOWN) {
		if (iBad >= 10) {
			sharpWriteString(2, y, "bus open error", FONT_8x8, 0);
		} else {
			sharpWriteString(2, y, "No sensors found", FONT_8x8, 0);
		}
		y += 8;
		sharpWriteString(2, y, "Press button to scan again", FONT_6x8, 0);
		sharpWriteBuffer();
		while (GetButtons() == 0) {
			Delay_Ms(25);
		}
		goto scan_again;
	}
	i = 0;
	sharpWriteString(118, 2, "Start", FONT_8x8, 0);
	sharpWriteString(110, 58, "Invert", FONT_8x8, 0);
	while (i == 0) { // wait for user to start or invert the colors
		sharpWriteBuffer();
		while (GetButtons() == 0) {
			Delay_Ms(25);
		}
		if (GetButtons() == 1) {
			while (GetButtons() != 0) {}; // wait for the user to release the button
			i = 1; // break out of loop and start sensing
		}
		if (GetButtons() == 2) {
			bInvert = ~bInvert;
			sharpInvert();
			sharpWriteBuffer();
			while (GetButtons() != 0) {
				// wait for button release
				Delay_Ms(25);
			}
		}
	} // while waiting for user to start
} /* ScanBus() */

void TIM2_PWMOut_Init(u16 arr, u16 psc, u16 ccp)
{
    GPIO_InitTypeDef GPIO_InitStructure={0};
    TIM_OCInitTypeDef TIM_OCInitStructure={0};
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure={0};

    RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOD , ENABLE );
    RCC_APB1PeriphClockCmd( RCC_APB1Periph_TIM2, ENABLE );

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 << (LCD_VCOM & 0xf);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init( GPIOD, &GPIO_InitStructure );

    TIM_TimeBaseInitStructure.TIM_Period = arr;
    TIM_TimeBaseInitStructure.TIM_Prescaler = psc;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit( TIM2, &TIM_TimeBaseInitStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM2;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = ccp;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC2Init( TIM2, &TIM_OCInitStructure );

    TIM_CtrlPWMOutputs(TIM2, ENABLE );
    TIM_OC2PreloadConfig( TIM2, TIM_OCPreload_Disable );
    TIM_ARRPreloadConfig( TIM2, ENABLE );
    TIM_Cmd( TIM2, ENABLE );
} /* TIM2_PWMOut_Init() */

void RunLTR390(void)
{
	int i, iMax, iHead = 0;
	int iSamples[32]; // keep about 3 seconds for the max value

	memset(iSamples, 0, sizeof(iSamples));
	ltr390_init(SDA_PIN, SCL_PIN, 400000);
	ltr390_start(1); // start UV sensor
    while(1)
    {
    //	digitalWrite(LED_PIN, 1);
    	ltr390_getSample();
    	iSamples[iHead++] = _iUV;
    	iHead &= 0x1f; // circular buffer
    	Delay_Ms(50); // default sample rate = 100ms
    //	digitalWrite(LED_PIN, 0);
//    	Delay_Ms(450);
    	// find the max value of the samples collected
    	iMax = 0;
    	for (i=0; i<32; i++) {
    		if (iSamples[i] > iMax) iMax = iSamples[i];
    	}
    	ShowLTR390Sample(_iUV, iMax);
    }

} /* RunLTR390() */

void ShowCO2(void)
{
	int i, x;
	char szTemp[32];

			sharpFill(bInvert);
	        i = i2str(szTemp, (int)_iCO2);
	        sharpWriteStringCustom(&Roboto_Black_40, 0, 32, szTemp, !bInvert, 1);
	        x = sharpGetCursorX();
	        if (i < 4) {
	           sharpWriteString(x+24, 0, "  ", FONT_12x16, bInvert); // make sure old data is erased if going from 4 to 3 digits
	           sharpWriteString(x, 16, "   ", FONT_12x16, bInvert);
	        }
	        sharpWriteString(x, 2, "CO2", FONT_8x8, bInvert);
	        sharpWriteString(x, 10, "ppm", FONT_8x8, bInvert);

	        sharpWriteString(2, 36, "Temp ", FONT_12x16, bInvert);
	        i2str(szTemp, _iTemperature/10); // whole part
	        sharpWriteString(-1, 36, szTemp, FONT_12x16, bInvert);
	        i2str(szTemp, _iTemperature % 10); // fraction
	        sharpWriteString(-1, 36, ".", FONT_12x16, bInvert);
	        sharpWriteString(-1, 36, szTemp, FONT_12x16, bInvert);
	        sharpWriteString(-1, 36, "C", FONT_12x16, bInvert);

	        sharpWriteString(2, 52, "Humidity ", FONT_12x16, bInvert);
	        i2str(szTemp, _iHumidity/10); // throw away fraction since it's not accurate
	        sharpWriteString(-1, 52, szTemp, FONT_12x16, bInvert);
	        sharpWriteString(-1, 52, "%", FONT_12x16, bInvert);
	        sharpWriteBuffer();
} /* ShowCO2() */

void ShowIMU(void)
{
	int16_t acc[3];
	char szTemp[16];

	IMUGetSample(acc, NULL, NULL); // get accelerometer samples
	sharpFill(0);
	sharpWriteString(2, 4, "X: ", FONT_12x16, 0);
	i2str(szTemp, acc[0]);
	sharpWriteString(-1, -1, szTemp, FONT_12x16, 0);

	sharpWriteString(2, 24, "Y: ", FONT_12x16, 0);
	i2str(szTemp, acc[1]);
	sharpWriteString(-1, -1, szTemp, FONT_12x16, 0);

	sharpWriteString(2, 44, "Z: ", FONT_12x16, 0);
	i2str(szTemp, acc[2]);
	sharpWriteString(-1, -1, szTemp, FONT_12x16, 0);
	sharpWriteBuffer();
} /* ShowIMU() */

void RunSCD4X(void)
{
    I2CInit(SDA_PIN, SCL_PIN, 100000);
    scd41_start(SCD_POWERMODE_NORMAL);
    while (1) {
        scd41_getSample();
        ShowCO2();
        Delay_Ms(5000); // 5 seconds per sample
    }
} /* RunSCD4X() */

void RunRTC(void)
{
	int iTick = 0;
	rtcInit((iSensor == SENSOR_DS3231) ? RTC_DS3231 : RTC_RV3032, SDA_PIN, SCL_PIN);
	rtcGetTime(&myTime);
	while (1) {
		if (GetButtons() == 1) {
			while (GetButtons() != 0) {};
			SetTime();
		} else if (GetButtons() == 2) {
			bInvert = ~bInvert;
			sharpInvert();
			sharpWriteBuffer();
			while (GetButtons() != 0) {};
		}
		Delay_Ms(33);
		if ((iTick & 0x1f) == 0) {
			ShowTime();
		}
		iTick++;
	}
} /* RunRTC() */

void RunIMU(void)
{
	IMUStart(200, 0, 0); // start accelerometer at 200 samples/sec
	while (1) {
		ShowIMU(); // run as fast as possible
	}
} /* RunIMU() */

int main(void)
{
	int i;
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    Delay_Init();
//    USART_Printf_Init(115200);
//    printf("SystemClk:%d\r\n",SystemCoreClock);

//    USARTx_CFG();
//    Delay_Ms(2000);
    TIM2_PWMOut_Init( 20, 65535, 10 ); // start a 50% duty cycle PWM output on D3 of about 4hz
    pinMode(BUTTON0_PIN, INPUT_PULLUP);
    pinMode(BUTTON1_PIN, INPUT_PULLUP);
    sharpInit(8000000, LCD_CS);
    sharpFill(0);
    sharpWriteBuffer();
    sharpWriteString(32, 2, "CH32V003", FONT_12x16, 0);
    sharpWriteString(44, 18, "Sensor", FONT_12x16, 0);
    sharpWriteString(32, 34, "Platform", FONT_12x16, 0);
    sharpWriteString(28, 50, "by Larry Bank", FONT_8x8, 0);
    sharpWriteString(2, 58,"Press buttons 1+2 to start", FONT_6x8, 0);
    sharpWriteBuffer();
//    pinMode(LCD_VCOM, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    i = 0;
    while (GetButtons() != 3) {
    	digitalWrite(LED_PIN, i & 1);
    	Delay_Ms(250);
    	i++;
    }
    digitalWrite(LED_PIN, 0);
    ScanBus(); // if we return from here, we have a recognized sensor
    digitalWrite(LED_PIN, 0);
    switch (iSensor) { // start displaying sensor data
    	case SENSOR_LSM6DS3:
    		RunIMU();
    		break;
    	case SENSOR_DS3231:
    	case SENSOR_RV3032:
    		RunRTC();
    		break;
    	case SENSOR_LTR390:
    		RunLTR390();
    		break;
    	case SENSOR_SCD4X:
    		RunSCD4X();
    		break;
    }
} /* main() */
