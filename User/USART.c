#include <stm32f10x.h>
#include "USART.h"


IOBuffer USART1_InQue = { 0 };


void USART1_Configuration(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    USART_InitTypeDef USART_InitStruct;

    RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOA
                            | RCC_APB2Periph_USART1, ENABLE);

    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    USART_InitStruct.USART_BaudRate = 38400;
    USART_InitStruct.USART_WordLength = USART_WordLength_8b;
    USART_InitStruct.USART_StopBits = USART_StopBits_1;
    USART_InitStruct.USART_Parity = USART_Parity_No;
    USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART1, &USART_InitStruct);

    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
    USART_Cmd(USART1, ENABLE);

    NVIC_EnableIRQ(USART1_IRQn);
}


void USART1_Send(uint8_t *data, int datalen)
{
    for (int i = 0; i < datalen; i++)
    {
        USART_ClearFlag(USART1, USART_FLAG_TC);
        USART_SendData(USART1, data[i]);
        while (USART_GetFlagStatus(USART1, USART_FLAG_TC) != SET);
    }
}


void USART1_IRQHandler(void)
{
    FlagStatus stat = USART_GetITStatus(USART1, USART_IT_RXNE);
    if (stat == SET)
    {
        IOB_BufferByte(&USART1_InQue, USART_ReceiveData(USART1));
        return;
    }

    stat = USART_GetFlagStatus(USART1, USART_FLAG_ORE);
    if (stat == SET)
    {
        USART_ClearFlag(USART1, USART_FLAG_ORE);
        USART_ReceiveData(USART1);
    }
}
