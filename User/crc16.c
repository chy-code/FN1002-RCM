

#include "crc16.h"


uint16_t
CalcCRC16(uint16_t initValue,
		const uint8_t *data, 
		uint32_t dataLen )
{
    const uint16_t POLYNOMIAL = 0x1021;
    uint16_t crc = initValue;
    uint16_t temp;

    for (uint32_t i = 0; i < dataLen; i++)
    {
        temp = data[i] << 8;
        for (int j = 8; j > 0; j--)
        {
            if ((temp ^ crc) & 0x8000)
            {
                crc <<= 1;
                crc ^= POLYNOMIAL;
            }
            else
            {
                crc <<= 1;
            }
            temp <<= 1;
        }
    }

    return crc;
}

