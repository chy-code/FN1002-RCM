#ifndef _LED_H
#define _LED_H

 // LED 类型定义
typedef enum {
	LED_ERROR,
	LED_STATUS,
	LED_RBFULL,
	LED_SLOT5,
	LED_SLOT4,
	LED_SLOT3,
	LED_SLOT2,
	LED_SLOT1,
	LED1,
	LED2,
	NUM_LEDs
} LEDType;

void LEDs_Configuration(void);

void LED_On(LEDType type);
void LED_Off(LEDType type);
void LED_ToggleState(LEDType type);

#endif
