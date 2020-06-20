
#include "RTL.h"
#include "stm32f10x.h"
#include "Stepper.h"


// 步进电机控制端口和引脚定义
const struct {
	GPIO_TypeDef* EN_Port;
	uint16_t EN_Pin;
	GPIO_TypeDef* DIR_Port;
	uint16_t DIR_Pin;
	GPIO_TypeDef* Plus_Port;
	uint16_t Plus_Pin;
	uint16_t TIM_IT;
} _cDrivers[] =
{
	{ GPIOA, GPIO_Pin_4, 	// EN
		GPIOA, GPIO_Pin_5 ,	// DIR
		GPIOA, GPIO_Pin_6 ,	// PLUS
		TIM_IT_CC1 }, // TIM_IT
	
	{ GPIOC, GPIO_Pin_4, // EN
		GPIOC, GPIO_Pin_5 , // DIR
		GPIOA, GPIO_Pin_7 , // PLUS
		TIM_IT_CC2},	// TIM_IT
};


// 步进电机控制参数
static struct {
	BOOL running;
	int stepsMax;
	int stepCount;
} _CB[] =
{ 
	{ __FALSE, 0, 0 },
	{ __FALSE, 0, 0 }
};


/*------------------------------------------------------------------
* 配置步进电机驱动器.
*------------------------------------------------------------------*/

static __forceinline void GPIO_Configuration(void)
{
	GPIO_InitTypeDef GPIO_InitStruct;
	
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	
	for (int i = 0; i < NUM_STEPPERS; i++)
	{
		GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
		GPIO_InitStruct.GPIO_Pin = _cDrivers[i].EN_Pin;
		GPIO_Init(_cDrivers[i].EN_Port, &GPIO_InitStruct);
		
		GPIO_InitStruct.GPIO_Pin = _cDrivers[i].DIR_Pin;
		GPIO_Init(_cDrivers[i].DIR_Port, &GPIO_InitStruct);
		
		GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
		GPIO_InitStruct.GPIO_Pin = _cDrivers[i].Plus_Pin;
		GPIO_Init(_cDrivers[i].Plus_Port, &GPIO_InitStruct);
		
		// Disable
		GPIO_ResetBits(_cDrivers[i].EN_Port, _cDrivers[i].EN_Pin);
	}
}


static __forceinline void TIM3_Configuration(void)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStruct;
	TIM_OCInitTypeDef TIM_OCInitStructure;
	
	TIM_TimeBaseInitStruct.TIM_Period = 65535;
	TIM_TimeBaseInitStruct.TIM_Prescaler = 72 - 1;
	TIM_TimeBaseInitStruct.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInitStruct.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseInitStruct);
	
	TIM_OCInitStructure.TIM_Pulse = 0;  
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM2;
 	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable; 
	TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Enable;
	TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_High;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Reset;
    TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCIdleState_Reset;
	
	TIM_OC1Init(TIM3, &TIM_OCInitStructure);
	TIM_OC2Init(TIM3, &TIM_OCInitStructure);
	
	TIM_OC1PreloadConfig(TIM3, TIM_OCPreload_Enable); 	
	TIM_OC2PreloadConfig(TIM3, TIM_OCPreload_Enable); 

	TIM_ARRPreloadConfig(TIM3, ENABLE);
}


void Steppers_Configuration(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA 
							| RCC_APB2Periph_GPIOC, ENABLE);
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

	GPIO_Configuration();
	TIM3_Configuration();

	NVIC_EnableIRQ(TIM3_IRQn);
}


void Stepper_Start(StepperType which, Direction dir, int nsteps)
{
	if (!_CB[which].running)
	{
		if (dir == DIR_FORWARD_OR_UP)
			GPIO_ResetBits( _cDrivers[which].DIR_Port, _cDrivers[which].DIR_Pin);
		else
			GPIO_SetBits( _cDrivers[which].DIR_Port, _cDrivers[which].DIR_Pin);
		
		GPIO_SetBits( _cDrivers[which].EN_Port, _cDrivers[which].EN_Pin);
		os_dly_wait(200);
		
		_CB[which].stepsMax = nsteps;
		_CB[which].stepCount = 0;
		_CB[which].running = __TRUE;
		
		TIM_SetAutoreload(TIM3, 200 - 1);	// CLK = 72000000/72/200 = 5 kHz
		TIM_SetCompare2(TIM3, 200 >> 1);
			
		TIM_ClearFlag(TIM3, TIM_FLAG_CC2);
		TIM_ITConfig(TIM3, TIM_IT_CC2, ENABLE);
	}
	
	if (!(TIM3->CR1 & TIM_CR1_CEN))
	{
		TIM_SetCounter(TIM3, 0);
		TIM_Cmd(TIM3, ENABLE);
	}
}


void Stepper_Stop(StepperType which)
{
	// 去能。置 EN 引脚为低电平。
	GPIO_ResetBits( _cDrivers[which].EN_Port,
					_cDrivers[which].EN_Pin);
	
	// 关闭相应通道的计数器中断
	TIM_ITConfig(TIM3, _cDrivers[which].TIM_IT, DISABLE);
	
	_CB[which].running = __FALSE;
	
	if ( ! (_CB[STEPPER_IN_OUT].running
			|| _CB[STEPPER_LIFTING].running) )
		TIM_Cmd(TIM3, DISABLE);
}


void Stepper_SetDirection(StepperType which, Direction dir)
{
	if (dir == DIR_FORWARD_OR_UP)
		GPIO_ResetBits(_cDrivers[which].DIR_Port, _cDrivers[which].DIR_Pin);
	else
		GPIO_SetBits(_cDrivers[which].DIR_Port, _cDrivers[which].DIR_Pin);
}


void Stepper_SetLowVelocity(StepperType which)
{
	TIM_SetAutoreload(TIM3, 500 - 1);
	TIM_SetCompare2(TIM3, 500 >> 1);
}


BOOL Stepper_IsRunning(StepperType which)
{
	return _CB[which].running;
}


// 定时器3 中断处理
void TIM3_IRQHandler(void)
{
	uint16_t TIM_IT;

	for (int i = 0; i < NUM_STEPPERS; i++)
	{
		if (_CB[i].running)
		{
			TIM_IT = _cDrivers[i].TIM_IT;
			
			if (TIM_GetITStatus(TIM3, TIM_IT) == SET)
			{
				TIM_ClearITPendingBit(TIM3, TIM_IT);
					
				_CB[i].stepCount++;
				if (_CB[i].stepCount >= _CB[i].stepsMax)
					Stepper_Stop((StepperType)i);
			}
		}
	}
}
