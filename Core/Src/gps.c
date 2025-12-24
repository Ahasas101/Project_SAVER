/*
 * gps.c
 *
 *  Created on: Sep 28, 2025
 *      Author: Ahasas Yadav
 */
#include "gps.h"




extern UART_HandleTypeDef huart6;
uint8_t RecBuff[50];
uint8_t TransBuff[] = "$EIGPQ,RMC*3A\r\n";
volatile uint8_t data;
volatile uint8_t data_pos = 0;
volatile Gps_Rec_Status RecStatus;
volatile uint8_t Rel_Data[10];

/*
 * Private functions
 */
static void GpsGetData(void);
static char* GpsGetRelevant(uint8_t pos);


static void GpsGetData(void)
{
	HAL_UART_Transmit(&huart6, TransBuff, strlen((char*)TransBuff), HAL_MAX_DELAY);
	RecStatus = RecFlagSet;
	HAL_UART_Receive_IT(&huart6,(uint8_t*)&data, 1);

}
//$GPRMC,,V,3128.86787,N,07611.25512,E,0.564,,250925,,,A*76


static char* GpsGetRelevant(uint8_t pos)
{
    static char field[20];
    uint8_t count = 0;
    uint8_t i = 0, j = 0;

    while (RecBuff[i] != '\0' && count < pos)
    {
        if (RecBuff[i] == ',')
        {
            count++;
            i++;
            continue;
        }

        if (count == pos - 1)
        {
            if (RecBuff[i] == ',' || RecBuff[i] == '\0')
                break;

            field[j++] = RecBuff[i];
        }
        i++;
    }

    field[j] = '\0';
    return field;
}

void GspInit(void)
{
	uint8_t baudcmd[] = "$PUBX,41,1,0007,0003,9600,0*10\r\n";
	HAL_UART_Transmit(&huart6, baudcmd, strlen((char*)baudcmd), HAL_MAX_DELAY);
	huart6.Init.BaudRate = 9600;
  if (HAL_UART_Init(&huart6) != HAL_OK)
  {
	Error_Handler();
  }

}

Gps_Status GpsGetStatus(void)
{
	GpsGetData();
	while(RecStatus != RecFlagClear);
	char *status = GpsGetRelevant(STATUSPOS);
	if (status != NULL && status[0] == 'V')
		return Void;
	else if(status != NULL && status[0] == 'A')
		return Active;

	return Error;

}

uint32_t GpsGetTime(void)
{
	GpsGetData();
	while(RecStatus != RecFlagClear);
	char *time = GpsGetRelevant(TIMEPOS);
	if(time != NULL)
	{
		return atof(time);
	}
	return -1;
}

uint32_t GpsGetLatitude(void)
{
	GpsGetData();
	while(RecStatus != RecFlagClear);
	char *lat = GpsGetRelevant(LATITUDEPOS);
	if(lat != NULL)
	{
		return atof(lat);
	}
	return -1;
}

uint32_t GpsGetLongitute(void)
{
	GpsGetData();
	while(RecStatus != RecFlagClear);
	char *longi = GpsGetRelevant(LONGITUDEPOS);
	if(longi != NULL)
	{
		return atof(longi);
	}
	return -1;
}

uint32_t GpsGetSpeed(void)
{
	GpsGetData();
	while(RecStatus != RecFlagClear);
	char *speed = GpsGetRelevant(LONGITUDEPOS);
	if(speed != NULL)
	{
		return atof(speed) * 1.852;
	}
	return -1;
}

uint32_t GpsGetDate(void)
{
	GpsGetData();
	while(RecStatus != RecFlagClear);
	char *date = GpsGetRelevant(LONGITUDEPOS);
	if(date != NULL)
	{
		return atof(date);
	}
	return -1;
}


void GpsCallbackHandler(void)
{

	if(data != '\n')
	{
		RecBuff[data_pos] = data;
		data_pos++;
		HAL_UART_Receive_IT(&huart6, (uint8_t*)&data, 1);

	}
	else
	{
		RecBuff[data_pos] = '\0';
		data_pos = 0;
		RecStatus = RecFlagClear;
	}
}
