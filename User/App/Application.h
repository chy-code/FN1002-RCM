#ifndef _APPLICATION_H
#define _APPLICATION_H

#include <stdint.h>
#include <RTL.h>

// 错误代码定义
#define ERR(name, code) name = code,
enum {
	#include "errors.h"
	ERR_LIM
};


// 卡片移动方式
typedef enum {
	MOVE_TO_SLOT,	// 吞卡(读卡器到卡槽)
	MOVE_TO_READER,	// 退卡到读卡器
	MOVE_TO_RETAINBIN	// 回收卡
} MoveMode;


// 设备状态
#define DEVICE_IDLE		0x08	// 空闲
#define DEVICE_BUSY 	0x04	// 忙
#define DEVICE_FAULT	0x20	// 故障

// 回收箱状态
#define RETAINBIN_FULL		0	// 满
#define RETAINBIN_NOT_FULL		1	// 未满
#define RETAINBIN_NOT_INPLACE	2	// 未安装到位

#define NUM_PHYSICAL_SLOTS	5 // 物理卡槽数
#define SLOT_INIT_POS	0x0f // 初始位置


// 应用状态
typedef struct {
	uint8_t deviceStat;	// 设备状态
	uint8_t currentSlot;	// 当前卡槽 0~4, 或 SLOT_INIT_POS
	int faultCode;		// 故障代码. 仅当 deviceStat 为 DEVICE_FAULT 时有效
	BOOL isMovingCard;	// 是否正在移动卡
	BOOL ejectFlag;		// 值为 TURE, 表示已执行退卡操作. FALSE 未执行
						// 在 Task_MoveCard 中更新此值
	BOOL reseted;		// 设备是否已复位
} AppStatus;


// 传感器数据
typedef struct {
	uint8_t retainBinStat;	// 回收箱状态
	uint8_t isCardInFront;	// 卡口处是否有卡 (1或0)
	uint8_t isRearCoverOpened; // 后盖们是否打开 (1或0)
	uint8_t isCardInSlots[NUM_PHYSICAL_SLOTS];	// 卡槽是否有卡 (1或0)
} SensorData;


// 性能测试数据
typedef struct {
	uint16_t UpTime[6];
	uint16_t DownTime[6];
	uint16_t TimePerRev1;
	uint16_t Reserved1[2];
	uint16_t TimePerRev2;
	uint16_t Reserved2[2];
} PerformanceData;


// 版本信息
typedef struct {
	char fw_ver[11];
	char hw_ver[11];
	char struct_ver[11];
} VersionData;


// 以下变量在 Application.c 中定义
extern const AppStatus * const c_appStatusRef;
extern const SensorData * const c_sensorDataRef;
extern const VersionData c_verData;

// 函数声明

int StartResetDevice(BOOL wait);
int StartSelectSlot(int targetSlot);

int StartMoveCard(MoveMode mode);
int StopMoveCard(void);

int StartPerformanceTest(PerformanceData *outData);

#endif
