#ifndef _STEPPER_H
#define _STEPPER_H

#ifndef __stdint_h
#include <stdint.h>
#endif

typedef enum {
	STEPPER_LIFTING,	// 升降用步进电机
	STEPPER_IN_OUT,	// 吞卡/退卡用电机
	NUM_STEPPERS
} StepperType;


typedef enum {
	DIR_FORWARD_OR_UP,	// 向前（读卡机方向）或向上
	DIR_BACKWARD_OR_DOWN	// 向后或向下
} Direction;


void Steppers_Configuration(void);
void Stepper_Start(StepperType which, Direction dir, int nsteps);
void Stepper_Stop(StepperType which);
void Stepper_SetDirection(StepperType which, Direction dir);
void Stepper_SetLowVelocity(StepperType which);
BOOL Stepper_IsRunning(StepperType which);

#endif
