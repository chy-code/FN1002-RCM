#include <stm32f10x.h>
#include <string.h>

#include "UserData.h"
#include "crc16.h"

#define PAGE_SIZE	1024

#define USER_ROM_BASE		0x0800F000
#define CP_BASE				(USER_ROM_BASE + 0)
#define CARDINFO_A_BASE		(USER_ROM_BASE + PAGE_SIZE )
#define SLOTS_FUNC_BASE		(USER_ROM_BASE + PAGE_SIZE * 9)
#define DEVICE_INFO_BASE	(USER_ROM_BASE + PAGE_SIZE * 10)
#define RETAIN_COUNTER_BASE	(USER_ROM_BASE + PAGE_SIZE * 11)
#define CARDINFO_B_BASE		(USER_ROM_BASE + PAGE_SIZE * 12)


const ControlParameters _defaultControlParameters =
{
    10, 20, 10, 20, 3, 5,
    40, 40, 0, 0,
    0, 0,
    40, 40, 1, 1,
    0, 0, 0, 0
};

const CardInfoA _defaultCardInfoA = { __FALSE, {0}, {0}, {0xAA, 0xBB, 0xCC} };
const CardInfoB _defaultCardInfoB = { {0}, {0} };
const uint16_t _defaultSlotsFunc[NUM_RCM_SLOTS] = { 1, 1, 1, 1, 1, 0, 0, 0 };
const uint8_t _defaultDeviceInfo[SZ_DEVICE_INFO] = { 0x20 };


BOOL InitPage(uint32_t base, const void *buf, size_t bytesToWrite);
BOOL WritePage(uint32_t base, const void *buf, size_t bytesToWrite);
BOOL ReadPage(uint32_t base, void *buf, size_t bytesToRead);


BOOL InitUserDataROM(void)
{
    if ( !InitPage( CP_BASE,
                    (uint8_t*)&_defaultControlParameters,
                    sizeof(ControlParameters)) )
        return __FALSE;

    for (int i = 0; i < NUM_RCM_SLOTS; i++)
    {
        if (!InitPage(CARDINFO_A_BASE + i * PAGE_SIZE,
                      &_defaultCardInfoA,
                      sizeof(CardInfoA)))
            return __FALSE;
    }

    if ( !InitPage( SLOTS_FUNC_BASE,
                    &_defaultSlotsFunc,
                    sizeof(_defaultSlotsFunc)) )
        return __FALSE;

    if ( !InitPage( DEVICE_INFO_BASE,
                    _defaultDeviceInfo,
                    sizeof(ControlParameters)) )
        return __FALSE;

    int retainCount = 0;
    if ( !InitPage( RETAIN_COUNTER_BASE,
                    &retainCount,
                    sizeof(int)) )
        return __FALSE;

    return __TRUE;
}


BOOL ProgramDefaultControlParameters(void)
{
    if (! WritePage( CP_BASE,
                     (uint8_t*)&_defaultControlParameters,
                     sizeof(ControlParameters) ))
        return __FALSE;

    return __TRUE;
}


BOOL ProgrmaControlParameters(ControlParameters *cp)
{
    if (! WritePage( CP_BASE,
                     (uint8_t*)cp,
                     sizeof(ControlParameters) ))
        return __FALSE;

    return __TRUE;
}


BOOL ProgramCardInfoA(int slotNum, CardInfoA *cardInfo)
{
    if (! WritePage( CARDINFO_A_BASE + slotNum * PAGE_SIZE,
                     cardInfo,
                     sizeof(CardInfoA)) )
        return __FALSE;

    return __TRUE;
}


BOOL ProgramSlotsFunc(uint16_t slotsFunc[NUM_RCM_SLOTS])
{
    if (! WritePage( SLOTS_FUNC_BASE,
                     slotsFunc,
                     2 * NUM_RCM_SLOTS) )
        return __FALSE;

    return __TRUE;
}


BOOL ProgramDeviceInfo(uint8_t deviceInfo[SZ_DEVICE_INFO])
{
    if (! WritePage( DEVICE_INFO_BASE,
                     deviceInfo,
                     SZ_DEVICE_INFO) )
        return __FALSE;

    return __TRUE;
}


BOOL ProgramRetainCount(int retainCount)
{
    if (! WritePage( RETAIN_COUNTER_BASE,
                     &retainCount,
                     sizeof(int)) )
        return __FALSE;

    return __TRUE;
}


BOOL ClearCardInfoA(int slotNum)
{
    if (! WritePage( CARDINFO_A_BASE + slotNum * PAGE_SIZE,
                     &_defaultCardInfoA,
                     sizeof(CardInfoA)) )
        return __FALSE;

    return __TRUE;
}


BOOL CleanCardInfoB(int index)
{
    if (! WritePage( CARDINFO_B_BASE + index * PAGE_SIZE,
                     &_defaultCardInfoB,
                     sizeof(CardInfoB)) )
        return __FALSE;

    return __TRUE;
}


BOOL ReadControlParameters(ControlParameters *cp)
{
    return ReadPage(CP_BASE, cp,
                    sizeof(ControlParameters));
}


BOOL ReadCardInfoA(int slotNum, CardInfoA *cardInfo)
{
    return ReadPage(CARDINFO_A_BASE + slotNum * PAGE_SIZE,
                    cardInfo,
                    sizeof(CardInfoA));
}


BOOL ReadCardInfoB(int index, CardInfoB *cardInfo)
{
    return __TRUE;
}


BOOL ReadSlotsFunc(uint16_t slotsFunc[NUM_RCM_SLOTS])
{
    return ReadPage(SLOTS_FUNC_BASE,
                    slotsFunc,
                    2 * NUM_RCM_SLOTS);
}


BOOL ReadDeviceInfo(uint8_t deviceInfo[SZ_DEVICE_INFO])
{
    return ReadPage(DEVICE_INFO_BASE,
                    deviceInfo,
                    SZ_DEVICE_INFO);
}


BOOL ReadRetainCount(int *retainCount)
{
    return ReadPage(RETAIN_COUNTER_BASE,
                    retainCount,
                    sizeof(int));
}


BOOL InitPage(uint32_t base, const void *buf, size_t bytesToWrite)
{
    uint8_t *data = (uint8_t*)(base + 2);
    uint16_t crc1 = *((uint16_t*)base);
    uint16_t crc2 = CalcCRC16(0, data, bytesToWrite);
    if (crc1 == crc2)
        return __TRUE;

    return WritePage(base, buf, bytesToWrite);
}


#define CLEAR_FLAGS	(FLASH_FLAG_BSY \
					| FLASH_FLAG_EOP \
					| FLASH_FLAG_PGERR \
					| FLASH_FLAG_WRPRTERR)


BOOL WritePage(uint32_t base, const void *buf, size_t bytesToWrite)
{
    FLASH_Unlock();
    FLASH_ClearFlag(CLEAR_FLAGS);

    FLASH_Status status = FLASH_ErasePage(base);
    if (status == FLASH_COMPLETE)
    {
        uint32_t addr = base + 2;
        uint32_t endAddr = addr + bytesToWrite;
        const uint16_t *data = buf;

        while (addr < endAddr)
        {
            status = FLASH_ProgramHalfWord(addr, *data++);
            if (status != FLASH_COMPLETE)
                break;

            addr += 2;
        }
    }

    if (status == FLASH_COMPLETE)
    {
        uint16_t crc = CalcCRC16(0, buf, bytesToWrite);
        status = FLASH_ProgramHalfWord(base, crc);
    }

    FLASH_Lock();

    if(status != FLASH_COMPLETE)
        return __FALSE;

    return __TRUE;
}


BOOL ReadPage(uint32_t base, void *buf, size_t bytesToRead)
{
    uint8_t *src = (uint8_t*)(base + 2);
    uint16_t crc1 = *((uint16_t*)base);
    uint16_t crc2 = CalcCRC16(0, src, bytesToRead);
    if (crc1 != crc2)
        return __FALSE;

    uint8_t *dest = (uint8_t*)buf;

    while (bytesToRead-- > 0)
        *dest++ = *src++;

    return __TRUE;
}

