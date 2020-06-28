#include <stm32f10x.h>
#include <string.h>

#include "UserData.h"
#include "crc16.h"

#define PAGE_SIZE			1024	// 页大小

#define PAGE_FLAG_SIZE		4
#define PAGE_FLAG_VALID		0xAABBCCDD


// 数据地址
#define USER_DATA_BASE		0x0800F000
#define CTRL_PARAMS_BASE	(USER_DATA_BASE + 0)
#define CARDINFO_BASE		(USER_DATA_BASE + PAGE_SIZE )
							// 预留8页,用于保存 CardInfo
							
#define SLOT_SETTINGS_BASE	(USER_DATA_BASE + PAGE_SIZE * 9)
#define DEVICE_INFO_BASE	(USER_DATA_BASE + PAGE_SIZE * 10)
#define RETAIN_COUNT_BASE	(USER_DATA_BASE + PAGE_SIZE * 11)


const ControlParameters _defaultControlParameters =
{
    10, 20, 10, 20, 3, 5,
    40, 40, 0, 0,
    0, 0,
    40, 40, 1, 1,
    0, 0, 0, 0
};

const CardInfo _defaultCardInfoA = { __FALSE, {0}, {0}, {0x30, 0x30, 0x30} };
const uint16_t _defaultSlotsFunc[NUM_RCM_SLOTS] = { 1, 1, 1, 1, 1, 0, 0, 0 };
const uint8_t _defaultDeviceInfo[SZ_DEVICE_INFO] = { 0x20 };


/*------------------------------------------------------------------
* FLASH 操作相关函数
*------------------------------------------------------------------*/

static BOOL EE_Read(uint32_t pageBase, void *var, size_t varSize);
static BOOL EE_PrepareRead(uint32_t pageBase, size_t varSize, uint32_t *addr);
static BOOL EE_Write(uint32_t pageBase, const void *var, size_t varSize);
static FLASH_Status EE_PrepareWrite(uint32_t pageBase, size_t bytesToWrite, uint32_t *addr);
static FLASH_Status EE_FormatPage(uint32_t pageBase);
static uint32_t EE_GetWriteAddress(uint32_t pageBase);
static BOOL EE_IsValidPage(uint32_t pageBase);


/*------------------------------------------------------------------
* 初始化用户数据 ROM.
* 如果数据页无效,写入默认值.
*------------------------------------------------------------------*/

BOOL InitUserDataROM(void)
{
    if (!EE_IsValidPage(CTRL_PARAMS_BASE))
    {
        if ( !EE_Write( CTRL_PARAMS_BASE,
                        (uint8_t*)&_defaultControlParameters,
                        sizeof(ControlParameters)) )
            return __FALSE;
    }

    for (int i = 0; i < NUM_RCM_SLOTS; i++)
    {
        if (!EE_IsValidPage(CARDINFO_BASE + i * PAGE_SIZE))
        {
            if (!EE_Write(CARDINFO_BASE + i * PAGE_SIZE,
                          &_defaultCardInfoA,
                          sizeof(CardInfo)))
                return __FALSE;
        }
    }

    if (!EE_IsValidPage(SLOT_SETTINGS_BASE))
    {
        if ( !EE_Write( SLOT_SETTINGS_BASE,
                        &_defaultSlotsFunc,
                        sizeof(_defaultSlotsFunc)) )
            return __FALSE;
    }

    if (!EE_IsValidPage(DEVICE_INFO_BASE))
    {
        if ( !EE_Write( DEVICE_INFO_BASE,
                        _defaultDeviceInfo,
                        sizeof(ControlParameters)) )
            return __FALSE;
    }

    int retainCount = 0;
    if (!EE_IsValidPage(RETAIN_COUNT_BASE))
    {
        if ( !EE_Write( RETAIN_COUNT_BASE,
                        &retainCount,
                        sizeof(int)) )
            return __FALSE;
    }

    return __TRUE;
}


/*------------------------------------------------------------------
* 向控制参数数据区写入默认参数.
*------------------------------------------------------------------*/

BOOL ProgramDefaultControlParameters(void)
{
    if (! EE_Write( CTRL_PARAMS_BASE,
                    (uint8_t*)&_defaultControlParameters,
                    sizeof(ControlParameters) ))
        return __FALSE;

    return __TRUE;
}


/*------------------------------------------------------------------
* 向控制参数数据区写入数据.
*------------------------------------------------------------------*/

BOOL ProgrmaControlParameters(ControlParameters *cp)
{
    if (! EE_Write( CTRL_PARAMS_BASE,
                    (uint8_t*)cp,
                    sizeof(ControlParameters) ))
        return __FALSE;

    return __TRUE;
}


/*------------------------------------------------------------------
* 向卡片信息区写入数据.
*------------------------------------------------------------------*/

BOOL ProgramCardInfoSlot(int slotNum, CardInfo *cardInfo)
{
    if (! EE_Write( CARDINFO_BASE + slotNum * PAGE_SIZE,
                    cardInfo,
                    sizeof(CardInfo)) )
        return __FALSE;

    return __TRUE;
}


/*------------------------------------------------------------------
* 向卡槽配置区写入数据.
*------------------------------------------------------------------*/

BOOL ProgramSlotSettings(uint16_t slotsFunc[NUM_RCM_SLOTS])
{
    if (! EE_Write( SLOT_SETTINGS_BASE,
                    slotsFunc,
                    2 * NUM_RCM_SLOTS) )
        return __FALSE;

    return __TRUE;
}


/*------------------------------------------------------------------
* 向设备信息区写入指定的数据.
*------------------------------------------------------------------*/

BOOL ProgramDeviceInfo(uint8_t deviceInfo[SZ_DEVICE_INFO])
{
    if (! EE_Write( DEVICE_INFO_BASE,
                    deviceInfo,
                    SZ_DEVICE_INFO) )
        return __FALSE;

    return __TRUE;
}


/*------------------------------------------------------------------
* 向回收计数区写入指定的数据.
*------------------------------------------------------------------*/

BOOL ProgramRetainCount(int retainCount)
{
    if (! EE_Write( RETAIN_COUNT_BASE,
                    &retainCount,
                    sizeof(int)) )
        return __FALSE;

    return __TRUE;
}


/*------------------------------------------------------------------
* 读控制参数.
*------------------------------------------------------------------*/

BOOL ReadControlParameters(ControlParameters *cp)
{
    return EE_Read(CTRL_PARAMS_BASE,
                   cp,
                   sizeof(ControlParameters));
}


/*------------------------------------------------------------------
* 读指定卡槽的卡片信息.
*------------------------------------------------------------------*/

BOOL ReadCardInfoSlot(int slotNum, CardInfo *cardInfo)
{
    return EE_Read(CARDINFO_BASE + slotNum * PAGE_SIZE,
                   cardInfo,
                   sizeof(CardInfo));
}


/*------------------------------------------------------------------
* 读卡槽配置.
*------------------------------------------------------------------*/

BOOL ReadSlotSettings(uint16_t slotsFunc[NUM_RCM_SLOTS])
{
    return EE_Read(SLOT_SETTINGS_BASE,
                   slotsFunc,
                   2 * NUM_RCM_SLOTS);
}


/*------------------------------------------------------------------
* 读设备信息.
*------------------------------------------------------------------*/

BOOL ReadDeviceInfo(uint8_t deviceInfo[SZ_DEVICE_INFO])
{
    return EE_Read(DEVICE_INFO_BASE,
                   deviceInfo,
                   SZ_DEVICE_INFO);
}


/*------------------------------------------------------------------
* 读回收计数.
*------------------------------------------------------------------*/

BOOL ReadRetainCount(int *retainCount)
{
    return EE_Read(RETAIN_COUNT_BASE,
                   retainCount,
                   sizeof(int));
}


/*------------------------------------------------------------------
* 读 FLASH.
* 参数: [in] pageBase   页地址
* 		[in] var		用于保存读取的数据
* 		[in] varSize	要读取的字节数
* 返回值: __TRUE 成功, __FLASE 失败.
*------------------------------------------------------------------*/

BOOL EE_Read(uint32_t pageBase, void *var, size_t varSize)
{
    uint32_t addr;
	
	// 获取待读取的地址
    if (!EE_PrepareRead(pageBase, varSize, &addr))
        return __FALSE;

    uint8_t *src = (uint8_t*)(addr);
    uint8_t *dest = (uint8_t*)var;

	// 校验
    uint16_t crc1 = *(uint16_t*)(addr + varSize);
    uint16_t crc2 = CalcCRC16(0, src, varSize);
    if (crc1 != crc2)
        return __FALSE; // 数据不完整

    while (varSize-- > 0)
        *dest++ = *src++;

    return __TRUE;
}


/*------------------------------------------------------------------
* 从页尾向页头查找最近写入的数据的起始地址. 在读数据之前调用.
* 参数:	[in ] pageBase 	页地址
* 		[in ] varSize	要读取的字节数
* 		[out] addr		最近写入的数据的起始地址
* 返回值: 函数返回 __TRUE, addr 有效.
*------------------------------------------------------------------*/

BOOL EE_PrepareRead(uint32_t pageBase, size_t varSize, uint32_t *addr)
{
    if (!EE_IsValidPage(pageBase))
        return __FALSE;

    uint32_t startAddr = pageBase + PAGE_FLAG_SIZE;

    *addr = EE_GetWriteAddress(pageBase);
    if (*addr == startAddr)
        return __FALSE; // 无数据

    *addr -= varSize + 2;
    if (*addr < startAddr)
        return __FALSE; // 地址不正确 (之前写入不正确)

    return __TRUE;
}


/*------------------------------------------------------------------
* 写 FLASH.
* 参数:	[in ] pageBase 	页地址
* 		[in ] var		要写入的数据
* 		[in ] varSize	要写入的数据的大小
* 返回值: __TRUE 成功, __FLASE 失败.
*------------------------------------------------------------------*/

BOOL EE_Write(uint32_t pageBase, const void *var, size_t varSize)
{
#define CLEAR_FLAGS	(FLASH_FLAG_BSY \
					| FLASH_FLAG_EOP \
					| FLASH_FLAG_PGERR \
					| FLASH_FLAG_WRPRTERR)

    uint32_t addr;
    uint16_t *dataStart;
    uint16_t *dataEnd;
    uint16_t crc;
    FLASH_Status status;

    FLASH_Unlock();
    FLASH_ClearFlag(CLEAR_FLAGS);

    status = EE_PrepareWrite(pageBase, varSize + 2, &addr);

    if (status == FLASH_COMPLETE)
    {
        dataStart = (uint16_t*)var;
        dataEnd = (uint16_t*)((uint8_t*)var + varSize);
        while (dataStart < dataEnd)
        {
            status = FLASH_ProgramHalfWord(addr, *dataStart++);
            if (status != FLASH_COMPLETE)
                break;
            addr += 2;
        }

        if (status == FLASH_COMPLETE)
        {
            // 在数据之后写入 CRC
            crc = CalcCRC16(0, var, varSize);
            status = FLASH_ProgramHalfWord(addr, crc);
        }
    }

    FLASH_Lock();

    if (status != FLASH_COMPLETE)
        return __FALSE;

    return __TRUE;
}


/*------------------------------------------------------------------
* 获取可完整写入指定大小数据的地址. 在写数据之前调用.
* 参数:	[in ] pageBase 	页地址
* 		[in ] bytesToWrite	要写入的数据的大小
* 		[out] addr			写数据的起始地址
* 返回值: FLASH_COMPLETE 成功, 其它值失败.
*------------------------------------------------------------------*/

FLASH_Status EE_PrepareWrite(uint32_t pageBase, size_t bytesToWrite, uint32_t *addr)
{
    uint8_t format = 0;
    size_t bytesCanWrite;
    FLASH_Status status;

    if (!EE_IsValidPage(pageBase))
    {
        // pageBase 页未格式化
        format = 1;
    }
    else
    {
        // 获取写入地址
        *addr = EE_GetWriteAddress(pageBase);
        bytesCanWrite = pageBase + PAGE_SIZE - *addr;
        if (bytesCanWrite < bytesToWrite)
        {
            // 剩余空间不足, 无法完整写入当前数据.
			// 需重新格式化, 然后从头开始写
            format = 1;
        }
    }

    status = FLASH_COMPLETE;

    if (format)
    {
        status = EE_FormatPage(pageBase);
        *addr = pageBase + PAGE_FLAG_SIZE;
    }

    return status;
}


/*------------------------------------------------------------------
* 擦除指定的页并写入标志.
*------------------------------------------------------------------*/

FLASH_Status EE_FormatPage(uint32_t pageAddr)
{
    FLASH_Status status = FLASH_ErasePage(pageAddr);
    if (status == FLASH_COMPLETE)
        status = FLASH_ProgramWord(pageAddr, PAGE_FLAG_VALID);

    return status;
}


/*------------------------------------------------------------------
* 在 pageBase 页中查找一个可写入数据的地址.
* 注意: 在写入数据之前需检查地址是否满足要求.
*------------------------------------------------------------------*/

uint32_t EE_GetWriteAddress(uint32_t pageBase)
{
    uint32_t addr = pageBase + PAGE_SIZE;
	
	// 从页尾向页首遍历, 如遇到不是 0xFFFF 的值, 则退出循环,
	// 且认为从此处开始可写入数据
    while (addr  > pageBase)
    {
		addr -= 2;
		
        if ( *((uint16_t*)addr) != 0xFFFF)
        {
            addr += 2;
            break;
        }
    }

    return addr;
}


BOOL EE_IsValidPage(uint32_t pageBase)
{
    uint32_t pageFlag = *((uint32_t*)pageBase);
    return pageFlag == PAGE_FLAG_VALID;
}
