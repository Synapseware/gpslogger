#ifndef __NMEA_PARSER_H__
#define __NMEA_PARSER_H__


#include <string.h>
#include <stdlib.h>
#include <inttypes.h>



/* Enable C linkage for C++ Compilers: */
#if defined(__cplusplus)
	extern "C" {
#endif


	/** Parses a NMEA sentence? */
	char* ParseNMEA_Sentence(const char* sentence);



#if defined(__cplusplus)
	}
#endif




#endif
