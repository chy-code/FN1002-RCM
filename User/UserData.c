#include <stm32f10x.h>
#include <string.h>

#include "UserData.h"
#include "crc16.h"

#define PAGE_SIZE			1024

#define PAGE_FLAG_SIZE		4
#define PAGE_FLAG_VALID		0xFFFFAABB


// 数据地址
#define USER_DATA_BASE		0x0800F000
#define CTRL_PARAMS_BASE	(USER_DATA_BASE + 0)
#define CARDINFO_BASE		(USER_DATA_BASE + PAGE_SIZE )
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


BOOL ProgramDefaultControlParameters(void)
{
    if (! EE_Write( CTRL_PARAMS_BASE,
                    (uint8_t*)&_defaultControlParameters,
                    sizeof(ControlParameters) ))
        return __FALSE;

    return __TRUE;
}


BOOL ProgrmaControlParameters(ControlParameters *cp)
{
    if (! EE_Write( CTRL_PARAMS_BASE,
                    (uint8_t*)cp,
                    sizeof(ControlParameters) ))
        return __FALSE;

    return __TRUE;
}


BOOL ProgramCardInfoSlot(int slotNum, CardInfo *cardInfo)
{
    if (! EE_Write( CARDINFO_BASE + slotNum * PAGE_SIZE,
                    cardInfo,
                    sizeof(CardInfo)) )
        return __FALSE;

    return __TRUE;
}


BOOL ProgramSlotSettings(uint16_t slotsFunc[NUM_RCM_SLOTS])
{
    if (! EE_Write( SLOT_SETTINGS_BASE,
                    slotsFunc,
                    2 * NUM_RCM_SLOTS) )
        return __FALSE;

    return __TRUE;
}


BOOL ProgramDeviceInfo(uint8_t deviceInfo[SZ_DEVICE_INFO])
{
    if (! EE_Write( DEVICE_INFO_BASE,
                    deviceInfo,
                    SZ_DEVICE_INFO) )
        return __FALSE;

    return __TRUE;
}


BOOL ProgramRetainCount(int retainCount)
{
    if (! EE_Write( RETAIN_COUNT_BASE,
                    &retainCount,
                    sizeof(int)) )
        return __FALSE;

    return __TRUE;
}


BOOL ReadControlParameters(ControlParameters *cp)
{
    return EE_Read(CTRL_PARAMS_BASE,
                   cp,
                   sizeof(ControlParameters));
}


BOOL ReadCardInfoSlot(int slotNum, CardInfo *cardInfo)
{
    return EE_Read(CARDINFO_BASE + slotNum * PAGE_SIZE,
                   cardInfo,
                   sizeof(CardInfo));
}


BOOL ReadSlotSettings(uint16_t slotsFunc[NUM_RCM_SLOTS])
{
    return EE_Read(SLOT_SETTINGS_BASE,
                   slotsFunc,
                   2 * NUM_RCM_SLOTS);
}


BOOL ReadDeviceInfo(uint8_t deviceInfo[SZ_DEVICE_INFO])
{
    return EE_Read(DEVICE_INFO_BASE,
                   deviceInfo,
                   SZ_DEVICE_INFO);
}


BOOL ReadRetainCount(int *retainCount)
{
    return EE_Read(RETAIN_COUNT_BASE,
                   retainCount,
                   sizeof(int));
}



BOOL EE_Read(uint32_t pageBase, void *var, size_t varSize)
{
    uint32_t addr;
    if (!EE_PrepareRead(pageBase, varSize, &addr))
        return __FALSE;

    uint8_t *src = (uint8_t*)(addr);
    uint8_t *dest = (uint8_t*)var;

    uint16_t crc1 = *(uint16_t*)(addr + varSize);
    uint16_t crc2 = CalcCRC16(0, src, varSize);
    if (crc1 != crc2)
        return __FALSE;

    while (varSize-- > 0)
        *dest++ = *src++;

    return __TRUE;
}


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
        return __FALSE; // 数据地址不正确

    return __TRUE;
}


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
            // 剩余空间不足, 重新格式化pageBase页
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


FLASH_Status EE_FormatPage(uint32_t pageAddr)
{
    FLASH_Status status = FLASH_ErasePage(pageAddr);
    if (status == FLASH_COMPLETE)
        status = FLASH_ProgramWord(pageAddr, PAGE_FLAG_VALID);

    return status;
}


uint32_t EE_GetWriteAddress(uint32_t pageBase)
{
    uint32_t addr = pageBase + PAGE_SIZE - 2;
    while (addr  > pageBase)
    {
        if ( *((uint16_t*)addr) != 0xFFFF)
        {
            addr += 2;
            break;
        }

        addr -= 2;
    }

    return addr;
}


BOOL EE_IsValidPage(uint32_t pageBase)
{
    uint32_t pageFlag = *((uint32_t*)pageBase);
    return pageFlag == PAGE_FLAG_VALID;
}
