#ifndef __GPS_H__
#define __GPS_H__


#include "gpslogger.h"


extern Adafruit_GPS GPS;


void initializeGPS(void);
void spoolData(void);
void enableGPS(void);
void disableGPS(void);
bool isEnabled(void);
char* parseGPSData(void);


#endif