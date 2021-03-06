
#include "stm32f10x.h"


// 复位按键硬件配置

#define RESET_BTN_PORT		GPIOA
#define RESET_BTN_PIN		GPIO_Pin_15

#define RESET_BTN_PORT_SOURCE	GPIO_PortSourceGPIOA
#define RESET_BTN_PIN_SOURCE	GPIO_PinSource15

#define RESET_BTN_EXTI_LINE		EXTI_Line15



void ResetButton_Enable(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	EXTI_InitTypeDef EXTI_InitStructure;
	
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

    GPIO_InitStructure.GPIO_Pin = RESET_BTN_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(RESET_BTN_PORT, &GPIO_InitStructure);
	
    GPIO_EXTILineConfig(RESET_BTN_PORT_SOURCE, RESET_BTN_PIN_SOURCE);

    EXTI_InitStructure.EXTI_Line = RESET_BTN_EXTI_LINE;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);
	
    NVIC_EnableIRQ(EXTI15_10_IRQn);
}


void EXTI15_10_IRQHandler(void)
{
    ITStatus stat = EXTI_GetITStatus(RESET_BTN_EXTI_LINE);
    if (stat != RESET)
    {
        __set_FAULTMASK(1);
        NVIC_SystemReset();
    }
}

