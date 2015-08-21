#incldue "NMEAParser.h"




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


/** Parses a NMEA sentence */
char* ParseNMEA_Sentence(const char* sentence)
{
	if (NULL == sentence)
	{
		return NULL;
	}

	if (strcmp(sentence, "$GPGGA", 6))
	{
		return ParsedNMEA_GPGGA(sentence);
	}

	if (strcmp(sentence, "$GPRMC", 6))
	{
		return ParsedNMEA_GPRMC(sentence);
	}

	return NULL;
}
