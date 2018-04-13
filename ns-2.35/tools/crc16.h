#ifndef ns_crc16_h
#define ns_crc16_h

#include <stdint.h>

class CRC_Generator
{
public:
	uint16_t gen_crc16(const uint8_t *data, uint16_t size);
}; 
#endif