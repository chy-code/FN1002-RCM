#ifndef _USERDATA_H
#define _USERDATA_H

#ifndef __RTL_H__
#include <RTL.h>
#endif

#ifndef __stdint_h
#include <stdint.h>
#endif

#define NUM_RCM_SLOTS			8

#define SZ_CARD_NUM				32
#define SZ_ID_INFO				20
#define SZ_DEVICE_INFO			30

#define RETAIN_CAPACITY_MAX	8

// 卡槽功能定义
#define SLOT_FUNC_DISABLE	0	// 不使用
#define SLOT_FUNC_IN_OUT	1	// 可以进卡和出卡
#define SLOT_FUNC_IN_ONLY	2	// 只能进卡


// 卡片信息
typedef struct {
	uint8_t valid;	// 卡号及身份信息是否有效 (4字节, RTL.h)
	uint8_t cardnum[SZ_CARD_NUM]; // 卡号
	uint8_t idinfo[SZ_ID_INFO];	// 身份信息
	uint8_t reserved[3];
} CardInfoA;


typedef struct {
	uint8_t cardnum[SZ_CARD_NUM]; // 卡号
	uint8_t idinfo[SZ_ID_INFO];	// 身份信息
} CardInfoB;


typedef struct {
	uint8_t moveOutDelay;
	uint8_t moveOutTimeOut;
	uint8_t moveInDelay;
	uint8_t moveInTimeOut;
	uint8_t retryTimes;
	uint8_t retainCapacity;
	uint8_t liftingParam1;
	uint8_t liftingParam2;
	uint8_t reserved1[4];
	uint8_t inOutParam1;
	uint8_t inOutParam2;
	uint8_t reserved2[6];
} ControlParameters;


BOOL InitUserDataROM(void);

BOOL ProgramDefaultControlParameters(void);
BOOL ProgrmaControlParameters(ControlParameters *cp);
BOOL ProgramCardInfoA(int slotNum, CardInfoA *cardInfo);
BOOL ProgramSlotsFunc(uint16_t slotsFunc[NUM_RCM_SLOTS]);
BOOL ProgramDeviceInfo(uint8_t deviceInfo[SZ_DEVICE_INFO]);
BOOL ProgramRetainCount(int retainCount);

BOOL ClearCardInfoA(int slotNum);
BOOL CleanCardInfoB(int index);

BOOL ReadControlParameters(ControlParameters *cp);
BOOL ReadCardInfoA(int slotNum, CardInfoA *cardInfo);
BOOL ReadCardInfoB(int index, CardInfoB *cardInfo);
BOOL ReadSlotsFunc(uint16_t slotsFunc[NUM_RCM_SLOTS]);
BOOL ReadDeviceInfo(uint8_t deviceInfo[SZ_DEVICE_INFO]);
BOOL ReadRetainCount(int *retainCount);

#endif

