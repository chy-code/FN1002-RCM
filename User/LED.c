#include "led.h"
#include "stm32f10x.h"

// LED GPIO 端口和引脚定义
const struct
{
    GPIO_TypeDef* GPIO;
    uint16_t GPIO_Pin;
} _cLEDs[] =
{
    // 保持定义顺序与 LEDType 顺序一致
    { GPIOB, GPIO_Pin_7 },	// PB7 LED ERR
    { GPIOB, GPIO_Pin_6 },	// PB6 LED STA
    { GPIOB, GPIO_Pin_5 },	// PB5 LED SRV
    { GPIOB, GPIO_Pin_4 },	// PB4 LED SLOT5
    { GPIOB, GPIO_Pin_3 },	// PB3 LED SLOT4
    { GPIOD, GPIO_Pin_2 },	// PD2 LED SLOT3
    { GPIOC, GPIO_Pin_12 },	// PC12 LED SLOT2
    { GPIOC, GPIO_Pin_11 },	// PC11 LED SLOT1
    { GPIOC, GPIO_Pin_0 },	// PC0	LED1
    { GPIOC, GPIO_Pin_1 },	// PC1	LED2
};


void LEDs_Configuration(void)
{
    RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOB
                            | RCC_APB2Periph_GPIOC
                            | RCC_APB2Periph_GPIOD
                            | RCC_APB2Periph_AFIO, ENABLE);

	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
	
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;

    for (int i = 0; i < NUM_LEDs; i++)
    {
        GPIO_InitStruct.GPIO_Pin = _cLEDs[i].GPIO_Pin;
        GPIO_Init(_cLEDs[i].GPIO, &GPIO_InitStruct);

        LED_On((LEDType)i);
    }
}


void LED_On(LEDType type)
{
    if (type == LED1 || type == LED2)
        GPIO_SetBits(_cLEDs[type].GPIO, _cLEDs[type].GPIO_Pin);
    else
        GPIO_ResetBits(_cLEDs[type].GPIO, _cLEDs[type].GPIO_Pin);
}


void LED_Off(LEDType type)
{
    if (type == LED1 || type == LED2)
        GPIO_ResetBits(_cLEDs[type].GPIO, _cLEDs[type].GPIO_Pin);
    else
        GPIO_SetBits(_cLEDs[type].GPIO, _cLEDs[type].GPIO_Pin);
}


void LED_ToggleState(LEDType type)
{
    GPIO_TypeDef *GPIO = _cLEDs[type].GPIO;
    GPIO->ODR ^= _cLEDs[type].GPIO_Pin;
}


