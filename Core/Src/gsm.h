/*
 * gsm.h
 *
 *  Created on: Oct 4, 2025
 *      Author: user
 */

#ifndef SRC_GSM_H_
#define SRC_GSM_H_

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "main.h"

#define TIMEOUT 		2000

typedef enum{
	GsmActive,
	GsmError
}Gsm_Status_def;

typedef enum{
	SimReady,
	SimLocked,
	SimPuk,
	NotPresent,
	SimFailure,
	UnknownError
}Sim_Status_def;

typedef enum{
	CallOk,
	NoCarrier,
	NoDialTone,
	CallBusy,
	NoAnswer,
	UnknownCmdError
}Call_Status_def;

typedef enum{
	Busy,
	Available

}Gsm_Rec_Status_def;

void GsmCallbackHandler(void);

void GsmInit(void);
Gsm_Status_def GsmGetStatus(void); // AT
Sim_Status_def GsmGetSimStatus(void); // AT+CPIN?
char* GsmGetSimICCID(void); // AT+CCID
Call_Status_def GsmCallNo(char* no); //ATD7017586549;
Call_Status_def GsmCallHangUp(void); //ATH
void GsmSendSMS(const char* number, const char* message);
void GsmConnectInternet(const char* apn);
void GsmHttpPost(const char* url, const char* data);

#endif /* SRC_GSM_H_ */
