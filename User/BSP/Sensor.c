
#include "stm32f10x.h"
#include "Sensor.h"


// 卡口传感器硬件配置
#define GATE_SEN_PORT		GPIOC
#define GATE_SEN_PIN		GPIO_Pin_7

// 后盖门传感器硬件配置
#define REAR_COVER_SEN_PORT		GPIOB
#define REAR_COVER_SEN_PIN		GPIO_Pin_12

// 凸轮传感器硬件配置
#define CAM_SEN_PORT		GPIOB
#define CAM_SEN_PIN			GPIO_Pin_13


// 74HC165 硬件配置
#define U74HC165_CLK_PORT		GPIOB
#define U74HC165_CLK_PIN		GPIO_Pin_15

#define U74HC165_Q7_PORT		GPIOC
#define U74HC165_Q7_PIN			GPIO_Pin_6

#define U74HC165_SHLD_PORT		GPIOB
#define U74HC165_SHLD_PIN		GPIO_Pin_14



static void U74HC165_SetNoChange(void);
static uint16_t U74HC165_Read(void);


void Sensors_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB
                           | RCC_APB2Periph_GPIOC, ENABLE);

    // 配置卡口传感器 GPIO
    GPIO_InitStruct.GPIO_Pin = GATE_SEN_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GATE_SEN_PORT, &GPIO_InitStruct);

    // 配置后盖门传感器 GPIO
    GPIO_InitStruct.GPIO_Pin = REAR_COVER_SEN_PIN;
    GPIO_Init(REAR_COVER_SEN_PORT, &GPIO_InitStruct);

    // 配置凸轮传感器 GPIO
    GPIO_InitStruct.GPIO_Pin = CAM_SEN_PIN;
    GPIO_Init(CAM_SEN_PORT, &GPIO_InitStruct);

    // 74HC165
    GPIO_InitStruct.GPIO_Pin = U74HC165_Q7_PIN;
    GPIO_Init(U74HC165_Q7_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.GPIO_Pin = U74HC165_SHLD_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(U74HC165_SHLD_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.GPIO_Pin = U74HC165_CLK_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(U74HC165_CLK_PORT, &GPIO_InitStruct);

    U74HC165_SetNoChange();
}


/*
* which 为传感器索引的组合.
* 如果相应传感器值为1, 则返回值中相应的传感器索引位为1.
*/

uint16_t Sensor_Read(uint16_t which)
{
    uint16_t data = 0;

    if (which & SEN_GATE)
        data |= GPIO_ReadInputDataBit(GATE_SEN_PORT, GATE_SEN_PIN) << 11;

    if (which & SEN_REAR_COVER)
        data |= GPIO_ReadInputDataBit(REAR_COVER_SEN_PORT, REAR_COVER_SEN_PIN) << 10;

    if (which & SEN_CAM)
        data |= GPIO_ReadInputDataBit(CAM_SEN_PORT, CAM_SEN_PIN) << 9;

    if (which & 0x1FF)
        data |= (U74HC165_Read() & which);

    return data;
}



void U74HC165_SetNoChange(void)
{
    GPIO_SetBits(U74HC165_SHLD_PORT, U74HC165_SHLD_PIN);
    GPIO_SetBits(U74HC165_CLK_PORT, U74HC165_CLK_PIN);
}



static __forceinline void delay(void)
{
    for (int i = 0; i < 100; i++);
}


uint16_t U74HC165_Read(void)
{
    uint16_t data = 0;

    // 置 SH/LD 为低电平
    GPIO_ResetBits(U74HC165_SHLD_PORT, U74HC165_SHLD_PIN);

    // 延时，等待数据加载到内部移位寄存器
    delay();

    // 置 SH/LD 为高电平
    GPIO_SetBits(U74HC165_SHLD_PORT, U74HC165_SHLD_PIN);

    // 读 Q7
    data = GPIO_ReadInputDataBit(U74HC165_Q7_PORT, U74HC165_Q7_PIN);

    // 将移位寄存器里的数据从Q7引脚串行输出
    for (int i = 0; i < 8; i++)
    {
        // 制造一次上升沿
        GPIO_ResetBits(U74HC165_CLK_PORT, U74HC165_CLK_PIN);
        delay();
        GPIO_SetBits(U74HC165_CLK_PORT, U74HC165_CLK_PIN);
        delay();

        data <<= 1;
        data |= GPIO_ReadInputDataBit(U74HC165_Q7_PORT, U74HC165_Q7_PIN);

        delay();
    }

    U74HC165_SetNoChange();

    return data;
}

