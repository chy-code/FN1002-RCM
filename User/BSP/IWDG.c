#include "stm32f10x.h"


void IWDG_Init(uint8_t prescaler, uint16_t reload)
{
	IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
	IWDG_SetPrescaler(prescaler);
	IWDG_SetReload(reload);
	IWDG_ReloadCounter();
	IWDG_Enable();
}


void IWDG_Feed(void)
{
	IWDG_ReloadCounter();
}
