#ifndef __FLASHTEST_H__
#define __FLASHTEST_H__


#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fstream>
#include <string.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <wordexp.h>

#include "FlashDriver.h"

using namespace std;

//the following are UBUNTU/LINUX, and MacOS ONLY terminal color codes.
#define RESET   "\033[0m"
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */
#define BLUE    "\033[34m"      /* Blue */
#define MAGENTA "\033[35m"      /* Magenta */
#define CYAN    "\033[36m"      /* Cyan */
#define WHITE   "\033[37m"      /* White */
#define BOLDBLACK   "\033[1m\033[30m"      /* Bold Black */
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
#define BOLDYELLOW  "\033[1m\033[33m"      /* Bold Yellow */
#define BOLDBLUE    "\033[1m\033[34m"      /* Bold Blue */
#define BOLDMAGENTA "\033[1m\033[35m"      /* Bold Magenta */
#define BOLDCYAN    "\033[1m\033[36m"      /* Bold Cyan */
#define BOLDWHITE   "\033[1m\033[37m"      /* Bold White */



void setErrLed(void);
void clearErrLed(void);
void setDbgLed(void);
void clearDbgLed(void);
bool writeString(uint32_t address, const char* buffer, int count);
int isPageFree(uint32_t address);
uint32_t findFirstFreeAddress(void);
string loadLogData(string path);
bool generateFlashImage(string path, const char* nmeaData);





#endif
