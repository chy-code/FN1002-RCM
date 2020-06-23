#ifndef _IWDG_H
#define _IWDG_H

#include <stdint.h>

#define IWDG_PRE_CYCLE_6P4	6	// 周期 6.4ms
#define IWDG_PRE_CYCLE_3P2	5	// 周期 3.2ms
#define IWDG_PRE_CYCLE_1P6	4	// 周期 1.6ms

void IWDG_Init(uint8_t prescaler, uint16_t reload);
void IWDG_Feed(void);

#endif
