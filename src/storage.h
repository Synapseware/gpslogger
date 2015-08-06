#ifndef __STORAGE_H__
#define __STORAGE_H__


#define USE_SD

#ifdef USE_SD
#include <SdVolume.h>
#include <SdStream.h>
#include <SdSpi.h>
#include <SdInfo.h>
#include <SdFile.h>
#include <SdFatUtil.h>
#include <SdFatStructs.h>
#include <SdFatmainpage.h>
#include <SdFatConfig.h>
#include <SdFat.h>
#include <SdBaseFile.h>
#include <Sd2Card.h>
#include <ostream.h>
#include <MinimumSerial.h>
#include <istream.h>
#include <iostream.h>
#include <ios.h>
#include <bufstream.h>
#endif

#include "gpslogger.h"


bool initializeSDCard(void);
void dateTime(uint16_t* date, uint16_t* time);
char* generateFilename(char* fileName);
void printLogData(void);
bool appendLogData(const char* nmeadata);


#endif