	/*
	 * sim.c
	 *
	 *  Created on: Oct 4, 2025
	 *      Author: Ahasas
	 */

	#include "gsm.h"
	 //   PA2     ------> USART2_TX
	   // PA3     ------> USART2_RX

	volatile uint8_t count = 0;

	extern UART_HandleTypeDef huart2;
	static void GsmSendCommand(char* command);
	static void GsmGetResponse(char* command);
	static volatile Gsm_Rec_Status_def RecStatus;
	static volatile uint8_t RecBuffer[100];
	static volatile uint8_t data;
	static volatile uint8_t nextlinecount = 0;
	static uint8_t data_pos = 0;
	static volatile uint32_t prev_tick = 0;


	static void GsmSendCommand(char* command)
	{
		HAL_UART_Transmit(&huart2, (uint8_t*)command, strlen(command), HAL_MAX_DELAY);
		HAL_Delay(200);
	}

	static void GsmGetResponse(char* command)
	{
		data_pos = 0;
		nextlinecount = 0;
		memset((char*)RecBuffer, 0, sizeof(RecBuffer));
		HAL_UART_Transmit(&huart2, (uint8_t*)command, strlen(command), HAL_MAX_DELAY);
		RecStatus = Busy;
		HAL_UART_Receive_IT(&huart2, (uint8_t*)&data, 1);
		HAL_Delay(TIMEOUT);

	}

	void GsmInit(void)
	{

		GsmSendCommand("AT+RST\r\n");
		GsmSendCommand("AT+IPR=9600\r\n");
		huart2.Init.BaudRate = 9600;
	  if (HAL_UART_Init(&huart2) != HAL_OK)
	  {
		Error_Handler();
	  }
		GsmSendCommand("ATE0\r\n");          // Disable echo
		GsmSendCommand("AT+CMEE=0\r\n");     // Disable verbose errors
		GsmSendCommand("AT+CREG=0\r\n");     // Disable network registration URC
		GsmSendCommand("AT+CGREG=0\r\n");    // Disable GPRS registration URC
		GsmSendCommand("AT+CNMI=0,0,0,0,0\r\n"); // Disable SMS notifications
		GsmSendCommand("AT+CLIP=0\r\n");     // Disable caller ID notifications
		//GsmSendCommand("AT&W\r\n");          // Save settings permanently



	}

	Gsm_Status_def GsmGetStatus(void)
	{
		prev_tick = HAL_GetTick();
		GsmGetResponse("AT\r\n");
		//while(RecStatus != Available);
		if (strcmp((char*)RecBuffer, "OK") == 0)
		{
			return GsmActive;
		}
		else
		{
			return GsmError;
		}


	}

	Sim_Status_def GsmGetSimStatus(void) // AT+CPIN?
	{
		prev_tick = HAL_GetTick();
		GsmGetResponse("AT+CPIN?\r\n");
		//while(RecStatus != Available);
		if (strcmp((char*)RecBuffer, "+CPIN:READYOK") == 0)
		{
			return SimReady;
		}
		else if (strcmp((char*)RecBuffer, "+CPIN:SIM PINOK") == 0)
		{
			return SimLocked;
		}
		else if (strcmp((char*)RecBuffer, "+CPIN:SIM PUKOK") == 0)
		{
			return SimPuk;
		}

		else if (strcmp((char*)RecBuffer, "+CPIN:NO SIMOK") == 0)
		{
			return NotPresent;
		}
		else if (strcmp((char*)RecBuffer, "+CPIN:SIM FAILUREOK") == 0)
		{
			return SimFailure;
		}
		else
		{
			return UnknownError;
		}



	}

	char* GsmGetSimICCID(void); // AT+CCID
	Call_Status_def GsmCallNo(char* no) //ATD7017586549;
	{
		char cmd[50];
		snprintf(cmd, sizeof(cmd), "ATD%s;\r\n", no);
		prev_tick = HAL_GetTick();
		GsmGetResponse(cmd);
		//while(RecStatus != Available);
		if (strcmp((char*)RecBuffer, "OK") == 0)
		{
			return CallOk;
		}
		else if (strcmp((char*)RecBuffer, "NO CARRIER") == 0)
		{
			return NoCarrier;
		}
		else if (strcmp((char*)RecBuffer, "NO DIALTONE") == 0)
		{
			return NoDialTone;
		}
		else if (strcmp((char*)RecBuffer, "BUSY") == 0)
		{
			return CallBusy;
		}
		else if (strcmp((char*)RecBuffer, "NO ANSWER") == 0)
		{
			return NoAnswer;
		}
		else
		{
			return UnknownCmdError;
		}
	}
	Call_Status_def GsmCallHangUp(void); //ATH

	void GsmSendSMS(const char* number, const char* message)
	{
		char cmd[50];
		char ctrlZ = 26;  // ASCII for Ctrl+Z

		// 1. Set SMS text mode
		const char *setTextMode = "AT+CMGF=1\r\n";
		HAL_UART_Transmit(&huart2, (uint8_t*)setTextMode, strlen(setTextMode), HAL_MAX_DELAY);
		HAL_Delay(500);

		// 2. Set receiver number
		sprintf(cmd, "AT+CMGS=\"%s\"\r\n", number);
		HAL_UART_Transmit(&huart2, (uint8_t*)cmd, strlen(cmd), HAL_MAX_DELAY);
		HAL_Delay(500);

		// 3. Send the message body
		HAL_UART_Transmit(&huart2, (uint8_t*)message, strlen(message), HAL_MAX_DELAY);
		HAL_Delay(100);

		// 4. Send Ctrl+Z to indicate end of message
		HAL_UART_Transmit(&huart2, (uint8_t*)&ctrlZ, 1, HAL_MAX_DELAY);
		HAL_Delay(5000); // wait for module to send message

	}


	void Sim800l_HttpPostJSON(const char* url, const char* apn, const char* json)
	{
	    char cmd[600];
	    int jsonLen = strlen(json);

	    /* 0. Set APN for GPRS */
	    snprintf(cmd, sizeof(cmd),
	             "AT+SAPBR=3,1,\"APN\",\"%s\"\r\n", apn);
	    GsmGetResponse(cmd);
	    HAL_Delay(400);

	    /* 1. Enable bearer */
	    GsmGetResponse("AT+SAPBR=1,1\r\n");
	    HAL_Delay(1000);

	    /* 2. Initialize HTTP service */
	    GsmGetResponse("AT+HTTPINIT\r\n");
	    HAL_Delay(300);

	    /* 3. Set HTTP bearer profile identifier */
	    GsmGetResponse("AT+HTTPPARA=\"CID\",1\r\n");
	    HAL_Delay(200);

	    /* 4. Set URL */
	    snprintf(cmd, sizeof(cmd),
	             "AT+HTTPPARA=\"URL\",\"%s\"\r\n", url);
	    GsmGetResponse(cmd);
	    HAL_Delay(400);

	    /* 5. Set content type to JSON */
	    GsmGetResponse("AT+HTTPPARA=\"CONTENT\",\"application/json\"\r\n");
	    HAL_Delay(200);

	    /* 6. Prepare data for POST */
	    snprintf(cmd, sizeof(cmd),
	             "AT+HTTPDATA=%d,10000\r\n", jsonLen);
	    GsmGetResponse(cmd);
	    HAL_Delay(300);

	    /* 7. Send the JSON body */
	    GsmSendCommand((char*)json);  // <- send body
	    HAL_Delay(200);

	    /* 8. Execute POST action */
	    GsmGetResponse("AT+HTTPACTION=1\r\n");
	    HAL_Delay(8000); // SIM800L needs time

	    /* 9. Read server response */
	    GsmGetResponse("AT+HTTPREAD\r\n");
	    HAL_Delay(1000);

	    /* 10. Terminate HTTP */
	    GsmGetResponse("AT+HTTPTERM\r\n");
	    HAL_Delay(200);

	    /* 11. Close bearer */
	    GsmGetResponse("AT+SAPBR=0,1\r\n");
	    HAL_Delay(300);
	}


	//void GsmCallbackHandler(void)
	//{ // /r/nOK/r/n
	//	count++;
	//	if(nextlinecount <= 2)
	//	{
	//		if(data == '\n' || data == '\r')
	//		{
	//			if(data == '\r') {nextlinecount++;}
	//		}
	//		else
	//		{
	//		RecBuffer[data_pos] = data;
	//		data_pos++;
	//		}
	//
	//		HAL_UART_Receive_IT(&huart2, (uint8_t*)&data, 1);
	//	}
	//
	//	else
	//	{
	//		nextlinecount = 0;
	//		RecBuffer[data_pos] = '\0';
	//		data_pos = 0;
	//		RecStatus = Available;
	//	}
	//}


	//Alternate Implementation

	// for previous tick you have to initilize the previous tick as global and volatile and change it just
	//before calling the receive api
	void GsmCallbackHandler(void)
	{

		if(data != '\r' && data != '\n')
		{
		RecBuffer[data_pos] = data;
		data_pos++;
		}
		HAL_UART_Receive_IT(&huart2, (uint8_t*)&data, 1);


	//		else
	//		{
	//			if(current_tick - prev_tick < TIMEOUT)
	//			{
	//				HAL_UART_Receive_IT(&huart2, (uint8_t*)&data, 1);
	//			}
	//
	//		}

	}
