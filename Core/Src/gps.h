/*
 * gps.h
 *
 *  Created on: Sep 28, 2025
 *      Author: Ahasas Yadav
 */

#ifndef SRC_GPS_H_
#define SRC_GPS_H_

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "main.h"

#define TIMEPOS			2
#define STATUSPOS		3
#define LATITUDEPOS		4
#define LONGITUDEPOS	6
#define SPEEDPOS		8
#define DATEPOS			10

typedef enum{
	Void,
	Active,
	Error
}Gps_Status;

typedef enum{
	RecFlagClear,
	RecFlagSet
}Gps_Rec_Status;

Gps_Status GpsGetStatus(void);
uint32_t GpsGetTime(void);
uint32_t GpsGetLatitude(void);
uint32_t GpsGetLongitute(void);
uint32_t GpsGetSpeed(void);
uint32_t GpsGetDate(void);
void GspInit(void);
void GpsCallbackHandler(void);


#endif /* SRC_GPS_H_ */
