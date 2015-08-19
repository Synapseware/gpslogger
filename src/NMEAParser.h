#ifndef __NMEA_PARSER_H__
#define __NMEA_PARSER_H__



int8_t ParseNMEA_Sentence(const char* sentence);

char* ParsedNMEA_GPGGA(void);
char* ParsedNMEA_GPRMC(void);
char* ParsedNMEA_PMTK(void);



#endif
