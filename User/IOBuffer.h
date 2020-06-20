#ifndef _IOBUFFER_H
#define _IOBUFFER_H

#ifndef __stdint_h
#include <stdint.h>
#endif

#define IOB_SIZE	1024
#define IOB_SIZE_MOD	(IOB_SIZE - 1)

typedef struct
{
    uint8_t buf[IOB_SIZE];
    uint16_t inPos;
    uint16_t outPos;
} IOBuffer;


__forceinline void IOB_BufferByte(IOBuffer *iob, uint8_t b)
{
    iob->buf[iob->inPos++] = b;
    iob->inPos &= IOB_SIZE_MOD;
}


__forceinline void IOB_BufferData(IOBuffer *iob, uint8_t *data, int datalen)
{
	register int i;
    for (i = 0; i < datalen; i++)
    {
        iob->buf[iob->inPos++] = data[i];
        iob->inPos &= IOB_SIZE_MOD;
    }
}

__forceinline void IOB_ReadByte(IOBuffer *iob, uint8_t *b)
{
    *b = iob->buf[iob->outPos++];
    iob->outPos &= IOB_SIZE_MOD;
}


__forceinline void IOB_ReadData(IOBuffer *iob, uint8_t *buf, int bytesToRead)
{
	register int i;
    for (i = 0; i < bytesToRead; i++)
    {
        buf[i] = iob->buf[iob->outPos++];
        iob->outPos &= IOB_SIZE_MOD;
    }
}


__forceinline uint16_t IOB_BytesCanRead(IOBuffer *iob)
{
    return (iob->inPos - iob->outPos) & IOB_SIZE_MOD;
}


#endif
