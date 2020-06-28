#ifndef _SENSOR_H
#define _SENSOR_H

#include <stdint.h>

// 传感器索引 
// (不要随意变更顺序)
#define SEN_GATE		0x800
#define SEN_REAR_COVER	0x400
#define SEN_CAM			0x200
#define SEN_RB_HIGH		0x100
#define SEN_SLOT1		0x80
#define SEN_SLOT2		0x40
#define SEN_SLOT3		0x20
#define SEN_SLOT4		0x10
#define SEN_SLOT5		0x8
#define SEN_BOTTOM		0x4
#define SEN_TOP			0x2
#define SEN_RB_INPLACE	0x1

#define SEN_ALL			0xFFF // 上面所有值之和

#define SEN_SLOTS	( SEN_SLOT1 \
					| SEN_SLOT2 \
					| SEN_SLOT3 \
					| SEN_SLOT4 \
					| SEN_SLOT5 )


void Sensors_Init(void);
uint16_t Sensor_Read(uint16_t which);

#endif
