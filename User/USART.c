#include <stm32f10x.h>
#include "USART.h"


IOBuffer USART1_InQue = { 0 };


static void USARTx_GPIO_Config(
    GPIO_TypeDef *GPIOPort,
    uint16_t TXPin,
    uint16_t RXPin )
{
    GPIO_InitTypeDef GPIO_InitStruct;

    GPIO_InitStruct.GPIO_Pin = TXPin;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOPort, &GPIO_InitStruct);

    GPIO_InitStruct.GPIO_Pin = RXPin;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOPort, &GPIO_InitStruct);
}


static void USARTx_Config(
    USART_TypeDef *USARTx,
    uint32_t BaudRate,
    uint16_t Parity)
{
    USART_InitTypeDef USART_InitStruct;

    USART_InitStruct.USART_BaudRate = BaudRate;
    USART_InitStruct.USART_WordLength = USART_WordLength_8b;
    USART_InitStruct.USART_StopBits = USART_StopBits_1;
    USART_InitStruct.USART_Parity = Parity;
    USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USARTx, &USART_InitStruct);

    USART_ITConfig(USARTx, USART_IT_RXNE, ENABLE);
    USART_Cmd(USARTx, ENABLE);
}


void USART1_Init(uint32_t baudRate, uint16_t parity)
{
    RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOA
                            | RCC_APB2Periph_USART1, ENABLE);

	USARTx_GPIO_Config(GPIOA, GPIO_Pin_9, GPIO_Pin_10);
	USARTx_Config(USART1, baudRate, parity);

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
