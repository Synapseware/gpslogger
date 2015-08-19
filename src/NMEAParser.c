#incldue "NMEAParser.h"



/** Parses a NMEA sentence */
char* ParseNMEA_Sentence(const char* sentence)
{
	if (NULL == sentence)
	{
		return NULL;
	}

	if (strstr(sentence, "$GPGGA", 6))
	{
		return ParsedNMEA_GPGGA(sentence);
	}

	if (strstr(sentence, "$GPRMC", 6))
	{
		return ParsedNMEA_GPRMC(sentence);
	}

	return NULL;
}

char* ParsedNMEA_GPGGA(void)
{
	return NULL;
}


char* ParsedNMEA_GPRMC(void)
{
	return NULL;
}


char* ParsedNMEA_PMTK(void)
{
	return NULL;
}


