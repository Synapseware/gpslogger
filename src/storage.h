#ifndef __STORAGE_H__
#define __STORAGE_H__


#include "gpslogger.h"


bool initializeSDCard(void);
void dateTime(uint16_t* date, uint16_t* time);
char* generateFilename(char* fileName);
void printLogData(void);
bool appendLogData(const char* nmeadata);


#endif