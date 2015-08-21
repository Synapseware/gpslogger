#include "NMEAParser.h"




char* ParsedNMEA_GPGGA(const char* sentence)
{
	return 0;
}


char* ParsedNMEA_GPRMC(const char* sentence)
{
	return 0;
}


/** Parses a NMEA sentence */
char* ParseNMEA_Sentence(const char* sentence)
{
	if (NULL == sentence)
	{
		return NULL;
	}

	if (strncmp(sentence, "$GPGGA", 6))
	{
		return ParsedNMEA_GPGGA(sentence);
	}

	if (strncmp(sentence, "$GPRMC", 6))
	{
		return ParsedNMEA_GPRMC(sentence);
	}

	return NULL;
}
