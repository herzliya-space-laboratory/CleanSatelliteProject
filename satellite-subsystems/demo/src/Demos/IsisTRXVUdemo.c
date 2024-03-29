/*
 * IsisTRXVUdemo.c
 *
 *  Created on: 6 feb. 2015
 *      Author: malv
 */

#include "common.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <at91/utility/exithandler.h>
#include <at91/commons.h>
#include <at91/utility/trace.h>
#include <at91/peripherals/cp15/cp15.h>

#include <hal/Utility/util.h>
#include <hal/Timing/WatchDogTimer.h>
#include <hal/Drivers/I2C.h>
#include <hal/Drivers/LED.h>
#include <hal/boolean.h>
#include <hal/errors.h>

#include <satellite-subsystems/IsisTRXVU.h>

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if USING_GOM_EPS == 1
#include <SatelliteSubsystems/GomEPS.h>
#endif

////General Variables
#define TX_UPBOUND				30
#define TIMEOUT_UPBOUND			10

#define SIZE_RXFRAME	30
#define SIZE_TXFRAME	235

// Test Function
static Boolean softResetVUTest(void)
{
	printf("\r\n Soft Reset of both receiver and transmitter microcontrollers \r\n");
	print_error(IsisTrxvu_componentSoftReset(0, trxvu_rc));
	vTaskDelay(1 / portTICK_RATE_MS);
	print_error(IsisTrxvu_componentSoftReset(0, trxvu_tc));

	return TRUE;
}

static Boolean hardResetVUTest(void)
{
	printf("\r\n Hard Reset of both receiver and transmitter microcontrollers \r\n");
	print_error(IsisTrxvu_componentHardReset(0, trxvu_rc));
	vTaskDelay(1 / portTICK_RATE_MS);
	print_error(IsisTrxvu_componentHardReset(0, trxvu_tc));

	return TRUE;
}

static Boolean vutc_sendDefClSignTest(void)
{
	//Buffers and variables definition
	unsigned char testBuffer1[10]  = {0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x40};
	unsigned char txCounter = 0;
	unsigned char avalFrames = 0;
	unsigned int timeoutCounter = 0;

	while(txCounter < 5 && timeoutCounter < 5)
	{
		printf("\r\n Transmission of single buffers with default callsign. AX25 Format. \r\n");
		print_error(IsisTrxvu_tcSendAX25DefClSign(0, testBuffer1, 10, &avalFrames));

		if ((avalFrames != 0)&&(avalFrames != 255))
		{
			printf("\r\n Number of frames in the buffer: %d  \r\n", avalFrames);
			txCounter++;
		}
		else
		{
			vTaskDelay(100 / portTICK_RATE_MS);
			timeoutCounter++;
		}
	}

	return TRUE;
}

static Boolean vutc_toggleIdleStateTest(void)
{
	static Boolean toggle_flag = 0;

	if(toggle_flag)
	{
	    print_error(IsisTrxvu_tcSetIdlestate(0, trxvu_idle_state_off));
		toggle_flag = FALSE;
	}
	else
	{
	    print_error(IsisTrxvu_tcSetIdlestate(0, trxvu_idle_state_on));
		toggle_flag = TRUE;
	}

	return TRUE;
}

static Boolean vutc_setTxBitrate9600Test(void)
{
    print_error(IsisTrxvu_tcSetAx25Bitrate(0, trxvu_bitrate_9600));

	return TRUE;
}

static Boolean vutc_setTxBitrate1200Test(void)
{
    print_error(IsisTrxvu_tcSetAx25Bitrate(0, trxvu_bitrate_1200));

	return TRUE;
}

static Boolean vurc_getFrameCountTest(void)
{
	unsigned short RxCounter = 0;
	unsigned int timeoutCounter = 0;

	while(timeoutCounter < 4*TIMEOUT_UPBOUND)
	{
	    print_error(IsisTrxvu_rcGetFrameCount(0, &RxCounter));

		timeoutCounter++;

		vTaskDelay(10 / portTICK_RATE_MS);
	}
	printf("\r\n There are currently %d frames waiting in the RX buffer \r\n", RxCounter);

	return TRUE;
}

static Boolean vurc_getFrameCmdTest(void)
{
	unsigned short RxCounter = 0;
	unsigned int i = 0;
	unsigned char rxframebuffer[SIZE_RXFRAME] = {0};
	ISIStrxvuRxFrame rxFrameCmd = {0,0,0, rxframebuffer};

	print_error(IsisTrxvu_rcGetFrameCount(0, &RxCounter));

	printf("\r\nThere are currently %d frames waiting in the RX buffer\r\n", RxCounter);

	while(RxCounter > 0)
	{
		print_error(IsisTrxvu_rcGetCommandFrame(0, &rxFrameCmd));

		printf("Size of the frame is = %d \r\n", rxFrameCmd.rx_length);

		printf("Frequency offset (Doppler) for received frame: %.2f Hz\r\n", ((double)rxFrameCmd.rx_doppler) * 13.352 - 22300.0); // Only valid for rev. B & C boards
		printf("Received signal strength (RSSI) for received frame: %.2f dBm\r\n", ((double)rxFrameCmd.rx_rssi) * 0.03 - 152);

		printf("The received frame content is = ");

		for(i = 0; i < rxFrameCmd.rx_length; i++)
		{
			printf("%02x ", rxFrameCmd.rx_framedata[i]);
		}
		printf("\r\n");

		print_error(IsisTrxvu_rcGetFrameCount(0, &RxCounter));

		vTaskDelay(10 / portTICK_RATE_MS);
	}

	return TRUE;
}

static Boolean vurc_getFrameCmdAndTxTest(void)
{
	unsigned short RxCounter = 0;
	unsigned int i = 0;
	unsigned char rxframebuffer[SIZE_RXFRAME] = {0};
	unsigned char trxvuBuffer = 0;
	ISIStrxvuRxFrame rxFrameCmd = {0,0,0, rxframebuffer};

	print_error(IsisTrxvu_rcGetFrameCount(0, &RxCounter));

	printf("\r\nThere are currently %d frames waiting in the RX buffer\r\n", RxCounter);

	while(RxCounter > 0)
	{
		print_error(IsisTrxvu_rcGetCommandFrame(0, &rxFrameCmd));

		printf("Size of the frame is = %d \r\n", rxFrameCmd.rx_length);

		printf("Frequency offset (Doppler) for received frame: %.2f Hz\r\n", ((double)rxFrameCmd.rx_doppler) * 13.352 - 22300.0); // Only valid for rev. B & C boards
		printf("Received signal strength (RSSI) for received frame: %.2f dBm\r\n", ((double)rxFrameCmd.rx_rssi) * 0.03 - 152);

		rxframebuffer[26] = '-';
		rxframebuffer[27] = 'O';
		rxframebuffer[28] = 'B';
		rxframebuffer[29] = 'C';

		IsisTrxvu_tcSendAX25DefClSign(0, rxframebuffer, SIZE_RXFRAME, &trxvuBuffer);

		printf("The received frame content is = ");

		for(i = 0; i < rxFrameCmd.rx_length; i++)
		{
			printf("%02x ", rxFrameCmd.rx_framedata[i]);
		}
		printf("\r\n");

		print_error(IsisTrxvu_rcGetFrameCount(0, &RxCounter));

		vTaskDelay(10 / portTICK_RATE_MS);
	}

	return TRUE;
}

static Boolean vurc_getRxTelemTest_revA(void)
{
	unsigned short telemetryValue;
	float eng_value = 0.0;
	ISIStrxvuRxTelemetry telemetry;
	int rv;

	// Telemetry values are presented as raw values
	printf("\r\nGet all Telemetry at once in raw values \r\n\r\n");
	rv = IsisTrxvu_rcGetTelemetryAll(0, &telemetry);
	if(rv)
	{
		printf("Subsystem call failed. rv = %d", rv);
		return TRUE;
	}

	telemetryValue = telemetry.fields.bus_volt;
	eng_value = ((float)telemetryValue) * 0.00488;
	printf("Bus voltage = %f \r\n", eng_value);

	telemetryValue = telemetry.fields.tx_current;
	eng_value = ((float)telemetryValue) * 0.0897;
	printf("Transmitter current = %f \r\n", eng_value);

	telemetryValue = telemetry.fields.rx_current;
	eng_value = ((float)telemetryValue) * 0.0305;
	printf("Receiver current = %f \r\n", eng_value);

	telemetryValue = telemetry.fields.pa_temp;
	eng_value = ((float)telemetryValue) * -0.0546 + 189.5522;
	printf("Pa temperature = %f \r\n", eng_value);

	telemetryValue = telemetry.fields.board_temp;
	eng_value = ((float)telemetryValue) * -0.0546 + 189.5522;
	printf("Board temperature = %f \r\n", eng_value);

	telemetryValue = telemetry.fields.rx_doppler;
	eng_value = ((float)telemetryValue) * 6.837 - 14000;
	printf("Receiver doppler = %f \r\n", eng_value);

	telemetryValue = telemetry.fields.rx_rssi;
	eng_value = ((float)telemetryValue * 0.03) - 152;
	printf("Receiver RSSI = %f \r\n", eng_value);

	return TRUE;
}


static Boolean vurc_getRxTelemTest_revB(void)
{
	unsigned short telemetryValue;
	float eng_value = 0.0;
	ISIStrxvuRxTelemetry telemetry;
	int rv;

	// Telemetry values are presented as raw values
	printf("\r\nGet all Telemetry at once in raw values \r\n\r\n");
	rv = IsisTrxvu_rcGetTelemetryAll(0, &telemetry);
	if(rv)
	{
		printf("Subsystem call failed. rv = %d", rv);
		return TRUE;
	}

	telemetryValue = telemetry.fields.bus_volt;
	eng_value = ((float)telemetryValue) * 0.00488;
	printf("Bus voltage = %f \r\n", eng_value);

	telemetryValue = telemetry.fields.tx_current;
	eng_value = ((float)telemetryValue) * 0.0897;
	printf("Transmitter current = %f \r\n", eng_value);

	telemetryValue = telemetry.fields.rx_current;
	eng_value = ((float)telemetryValue) * 0.0305;
	printf("Receiver current = %f \r\n", eng_value);

	telemetryValue = telemetry.fields.pa_temp;
	eng_value = ((float)telemetryValue) * -0.0546 + 189.5522;
	printf("Pa temperature = %f \r\n", eng_value);

	telemetryValue = telemetry.fields.board_temp;
	eng_value = ((float)telemetryValue) * -0.0546 + 189.5522;
	printf("Board temperature = %f \r\n", eng_value);

	telemetryValue = telemetry.fields.rx_doppler;
	eng_value = ((float)telemetryValue) * 13.352 - 22300;
	printf("Receiver doppler = %f \r\n", eng_value);

	telemetryValue = telemetry.fields.rx_rssi;
	eng_value = ((float)telemetryValue) * 0.03 - 152;
	printf("Receiver RSSI = %f \r\n", eng_value);

	return TRUE;
}

static Boolean vurc_getRxTelemTest_revC(void)
{
	unsigned short telemetryValue;
	float eng_value = 0.0;
	ISIStrxvuRxTelemetry_revC telemetry;
	int rv;

	// Telemetry values are presented as raw values
	printf("\r\nGet all Telemetry at once in raw values \r\n\r\n");
	rv = IsisTrxvu_rcGetTelemetryAll_revC(0, &telemetry);
	if(rv)
	{
		printf("Subsystem call failed. rv = %d", rv);
		return TRUE;
	}

	telemetryValue = telemetry.fields.bus_volt;
	eng_value = ((float)telemetryValue) * 0.00488;
	printf("Bus voltage = %f V\r\n", eng_value);

	telemetryValue = telemetry.fields.total_current;
	eng_value = ((float)telemetryValue) * 0.16643964;
	printf("Total current = %f mA\r\n", eng_value);

	telemetryValue = telemetry.fields.pa_temp;
	eng_value = ((float)telemetryValue) * -0.07669 + 195.6037;
	printf("PA temperature = %f degC\r\n", eng_value);

	telemetryValue = telemetry.fields.locosc_temp;
	eng_value = ((float)telemetryValue) * -0.07669 + 195.6037;
	printf("Local oscillator temperature = %f degC\r\n", eng_value);

	telemetryValue = telemetry.fields.rx_doppler;
	eng_value = ((float)telemetryValue) * 13.352 - 22300;
	printf("Receiver doppler = %f Hz\r\n", eng_value);

	telemetryValue = telemetry.fields.rx_rssi;
	eng_value = ((float)telemetryValue) * 0.03 - 152;
	printf("Receiver RSSI = %f dBm\r\n", eng_value);

	return TRUE;
}

static Boolean vutc_getTxTelemTest_revA(void)
{
	unsigned short telemetryValue;
	float eng_value = 0.0;
	ISIStrxvuTxTelemetry telemetry;
	int rv;

	// Telemetry values are presented as raw values
	printf("\r\nGet all Telemetry at once in raw values \r\n\r\n");
	rv = IsisTrxvu_tcGetTelemetryAll(0, &telemetry);
	if(rv)
	{
		printf("Subsystem call failed. rv = %d", rv);
		return TRUE;
	}

	telemetryValue = telemetry.fields.tx_current;
	eng_value = ((float)telemetryValue) * 0.0897;
	printf("Transmitter current is = %f \r\n", eng_value);

	telemetryValue = telemetry.fields.pa_temp;
	eng_value = ((float)telemetryValue) * -0.0546 + 189.5522;
	printf("PA temperature is = %f \r\n", eng_value);

	telemetryValue = telemetry.fields.tx_reflpwr;
	eng_value = ((float)(telemetryValue * telemetryValue)) * 0.00005887;
	printf("RF reflected power is = %f \r\n", eng_value);

	telemetryValue = telemetry.fields.tx_fwrdpwr;
	eng_value = ((float)(telemetryValue * telemetryValue)) * 0.00005887;
	printf("RF forward power is = %f \r\n", eng_value);

	return TRUE;
}

static Boolean vutc_getTxTelemTest_revB(void)
{
	unsigned short telemetryValue;
	float eng_value = 0.0;
	ISIStrxvuTxTelemetry telemetry;
	int rv;

	// Telemetry values are presented as raw values
	printf("\r\nGet all Telemetry at once in raw values \r\n\r\n");
	rv = IsisTrxvu_tcGetTelemetryAll(0, &telemetry);
	if(rv)
	{
		printf("Subsystem call failed. rv = %d", rv);
		return TRUE;
	}

	telemetryValue = telemetry.fields.tx_current;
	eng_value = ((float)telemetryValue) * 0.0897;
	printf("Transmitter current is = %f \r\n", eng_value);

	telemetryValue = telemetry.fields.pa_temp;
	eng_value = ((float)telemetryValue) * -0.0546 + 189.5522;
	printf("PA temperature is = %f \r\n", eng_value);

	telemetryValue = telemetry.fields.tx_reflpwr;
	eng_value = ((float)(telemetryValue * telemetryValue)) * 0.00005887;
	printf("RF reflected power is = %f \r\n", eng_value);

	telemetryValue = telemetry.fields.tx_fwrdpwr;
	eng_value = ((float)(telemetryValue * telemetryValue)) * 0.00005887;
	printf("RF forward power is = %f \r\n", eng_value);

	return TRUE;
}

static Boolean vutc_getTxTelemTest_revC(void)
{
	unsigned short telemetryValue;
	float eng_value = 0.0;
	ISIStrxvuTxTelemetry_revC telemetry;
	int rv;

	// Telemetry values are presented as raw values
	printf("\r\nGet all Telemetry at once in raw values \r\n\r\n");
	rv = IsisTrxvu_tcGetTelemetryAll_revC(0, &telemetry);
	if(rv)
	{
		printf("Subsystem call failed. rv = %d", rv);
		return TRUE;
	}

	telemetryValue = telemetry.fields.bus_volt;
	eng_value = ((float)telemetryValue) * 0.00488;
	printf("Bus voltage = %f V\r\n", eng_value);

	telemetryValue = telemetry.fields.total_current;
	eng_value = ((float)telemetryValue) * 0.16643964;
	printf("Total current = %f mA\r\n", eng_value);

	telemetryValue = telemetry.fields.pa_temp;
	eng_value = ((float)telemetryValue) * -0.07669 + 195.6037;
	printf("PA temperature = %f degC\r\n", eng_value);

	telemetryValue = telemetry.fields.tx_reflpwr;
	eng_value = ((float)(telemetryValue * telemetryValue)) * 5.887E-5;
	printf("RF reflected power = %f mW\r\n", eng_value);

	telemetryValue = telemetry.fields.tx_fwrdpwr;
	eng_value = ((float)(telemetryValue * telemetryValue)) * 5.887E-5;
	printf("RF forward power = %f mW\r\n", eng_value);

	telemetryValue = telemetry.fields.locosc_temp;
	eng_value = ((float)telemetryValue) * -0.07669 + 195.6037;
	printf("Local oscillator temperature = %f degC\r\n", eng_value);

	return TRUE;
}

static Boolean selectAndExecuteTRXVUDemoTest(void)
{
	unsigned int selection = 0;
	Boolean offerMoreTests = TRUE;

	printf( "\n\r Select a test to perform: \n\r");
	printf("\t 1) Soft Reset TRXVU both microcontrollers \n\r");
	printf("\t 2) Hard Reset TRXVU both microcontrollers \n\r");
	printf("\t 3) Default Callsign Send Test\n\r");
	printf("\t 4) Toggle Idle state \n\r");
	printf("\t 5) Change transmission bitrate to 9600  \n\r");
	printf("\t 6) Change transmission bitrate to 1200 \n\r");
	printf("\t 7) Get frame count \n\r");
	printf("\t 8) Get command frame \n\r");
	printf("\t 9) Get command frame and retransmit \n\r");
	printf("\t 10)  (revA) Get receiver telemetry \n\r");
	printf("\t 11) (revA) Get transmitter telemetry \n\r");
	printf("\t 12) (revB) Get receiver telemetry \n\r");
	printf("\t 13) (revB) Get transmitter telemetry \n\r");
	printf("\t 14) (revC) Get receiver telemetry \n\r");
	printf("\t 15) (revC) Get transmitter telemetry \n\r");
	printf("\t 16) Return to main menu \n\r");

	while(UTIL_DbguGetIntegerMinMax(&selection, 1, 16) == 0);

	switch(selection) {
	case 1:
		offerMoreTests = softResetVUTest();
		break;
	case 2:
		offerMoreTests = hardResetVUTest();
		break;
	case 3:
		offerMoreTests = vutc_sendDefClSignTest();
		break;
	case 4:
		offerMoreTests = vutc_toggleIdleStateTest();
		break;
	case 5:
		offerMoreTests = vutc_setTxBitrate9600Test();
		break;
	case 6:
		offerMoreTests = vutc_setTxBitrate1200Test();
		break;
	case 7:
		offerMoreTests = vurc_getFrameCountTest();
		break;
	case 8:
		offerMoreTests = vurc_getFrameCmdTest();
		break;
	case 9:
		offerMoreTests = vurc_getFrameCmdAndTxTest();
		break;
	case 10:
		offerMoreTests = vurc_getRxTelemTest_revA();
		break;
	case 11:
		offerMoreTests = vutc_getTxTelemTest_revA();
		break;
	case 12:
		offerMoreTests = vurc_getRxTelemTest_revB();
		break;
	case 13:
		offerMoreTests = vutc_getTxTelemTest_revB();
		break;
	case 14:
		offerMoreTests = vurc_getRxTelemTest_revC();
		break;
	case 15:
		offerMoreTests = vutc_getTxTelemTest_revC();
		break;
	case 16:
		offerMoreTests = FALSE;
		break;

	default:
		break;
	}

	return offerMoreTests;
}

Boolean IsisTRXVUdemoInit(void)
{
    // Definition of I2C and TRXUV
	ISIStrxvuI2CAddress myTRXVUAddress[1];
	ISIStrxvuFrameLengths myTRXVUBuffers[1];
	ISIStrxvuBitrate myTRXVUBitrates[1];
    int rv;

	//I2C addresses defined
	myTRXVUAddress[0].addressVu_rc = 0x60;
	myTRXVUAddress[0].addressVu_tc = 0x61;

	//Buffer definition
	myTRXVUBuffers[0].maxAX25frameLengthTX = SIZE_TXFRAME;
	myTRXVUBuffers[0].maxAX25frameLengthRX = SIZE_RXFRAME;

	//Bitrate definition
	myTRXVUBitrates[0] = trxvu_bitrate_1200;

	//Initialize the trxvu subsystem
	rv = IsisTrxvu_initialize(myTRXVUAddress, myTRXVUBuffers, myTRXVUBitrates, 1);
	if(rv != E_NO_SS_ERR && rv != E_IS_INITIALIZED)
	{
		// we have a problem. Indicate the error. But we'll gracefully exit to the higher menu instead of
		// hanging the code
		TRACE_ERROR("\n\r IsisTrxvu_initialize() failed; err=%d! Exiting ... \n\r", rv);
		return FALSE;
	}

	return TRUE;
}

void IsisTRXVUdemoLoop(void)
{
	Boolean offerMoreTests = FALSE;

	while(1)
	{
		offerMoreTests = selectAndExecuteTRXVUDemoTest();	// show the demo command line interface and handle commands

		if(offerMoreTests == FALSE)							// was exit/back selected?
		{
			break;
		}
	}
}

Boolean IsisTRXVUdemoMain(void)
{
	if(IsisTRXVUdemoInit())									// initialize of I2C and IsisTRXVU subsystem drivers succeeded?
	{
		IsisTRXVUdemoLoop();								// show the main IsisTRXVU demo interface and wait for user input
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

Boolean TRXVUtest(void)
{
	IsisTRXVUdemoMain();
	return TRUE;
}
