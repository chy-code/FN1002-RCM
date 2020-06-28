#ifndef _CRC16_H
#define _CRC16_H

#ifndef __stdint_h
#include <stdint.h>
#endif

uint16_t
CalcCRC16(uint16_t initValue,
		const uint8_t *data, 
		uint32_t dataLen );

#endif
