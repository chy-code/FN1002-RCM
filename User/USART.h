#ifndef _USART_H
#define _USART_H

#include "IOBuffer.h"

extern IOBuffer USART1_InQue;

void USART1_Configuration(void);
void USART1_Send(uint8_t *data, int datalen);


#endif
