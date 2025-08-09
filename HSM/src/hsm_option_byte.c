#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_flash_ex.h"
#include "hsm_option_byte.h"

RetStatus setRDPLevelOne()
{
    FLASH_OBProgramInitTypeDef ob_init;

    /* Unlock FLASH & Option Bytes */
    HAL_FLASH_Unlock();
    HAL_FLASH_OB_Unlock();

    /* Initialize RDP configuration */
    // RDP LEVEL_0: Disable protection
    // RDP LEVEL_1: Enable readout protection
    // RDP LEVEL_2: Permanent protection (Warning!)
    ob_init.OptionType = OPTIONBYTE_RDP | OPTIONBYTE_BOR;
    ob_init.RDPLevel = OB_RDP_LEVEL_1;
    ob_init.BORLevel = OB_BOR_LEVEL3; // Supply voltage ranges from 2.70 to 3.60 V

    /* Apply configuration */
    HAL_FLASHEx_OBProgram(&ob_init);

    /* Launch Option Bytes configuration */
    HAL_FLASH_OB_Launch();

    /* Lock FLASH & Option Bytes */
    HAL_FLASH_OB_Lock();
    HAL_FLASH_Lock();

    return RET_OK;
}

#if defined(DISABLE_RDP_LEVEL_TWO)
RetStatus setRDPLevelTwo()
{
    FLASH_OBProgramInitTypeDef ob_init;

    /* Unlock FLASH & Option Bytes */
    HAL_FLASH_Unlock();
    HAL_FLASH_OB_Unlock();

    /* Initialize RDP configuration */
    // RDP LEVEL_0: Disable protection
    // RDP LEVEL_1: Enable readout protection
    // RDP LEVEL_2: Permanent protection (Warning!)
    ob_init.OptionType = OPTIONBYTE_RDP | OPTIONBYTE_BOR;
    ob_init.RDPLevel = OB_RDP_LEVEL_2;
    ob_init.BORLevel = OB_BOR_LEVEL3; // Supply voltage ranges from 2.70 to 3.60 V

    /* Apply configuration */
    HAL_FLASHEx_OBProgram(&ob_init);

    /* Launch Option Bytes configuration */
    HAL_FLASH_OB_Launch();

    /* Lock FLASH & Option Bytes */
    HAL_FLASH_OB_Lock();
    HAL_FLASH_Lock();

    return RET_OK;
}
#endif

uint32_t enableWRP(uint32_t wrp_sectors)
{
    FLASH_OBProgramInitTypeDef ob_init;
    uint32_t status = HAL_FLASH_ERROR_NONE;

    HAL_FLASH_Unlock();
    HAL_FLASH_OB_Unlock();

    ob_init.OptionType = OPTIONBYTE_WRP;
    ob_init.WRPState   = OB_WRPSTATE_ENABLE;
    ob_init.WRPSector  = wrp_sectors;
    ob_init.Banks      = FLASH_BANK_1;

    if (HAL_FLASHEx_OBProgram(&ob_init) != HAL_OK)
    {
        status = HAL_FLASH_GetError();
    }
    else
    {
        HAL_FLASH_OB_Launch();
    }

    HAL_FLASH_OB_Lock();
    HAL_FLASH_Lock();

    return status;
}

uint32_t disableWRP(uint32_t wrp_sectors)
{
    FLASH_OBProgramInitTypeDef ob_init;
    uint32_t status = HAL_FLASH_ERROR_NONE;

    HAL_FLASH_Unlock();
    HAL_FLASH_OB_Unlock();

    ob_init.OptionType = OPTIONBYTE_WRP;
    ob_init.WRPState   = OB_WRPSTATE_DISABLE;
    ob_init.WRPSector  = wrp_sectors;
    ob_init.Banks      = FLASH_BANK_1;

    if (HAL_FLASHEx_OBProgram(&ob_init) != HAL_OK)
    {
        status = HAL_FLASH_GetError();
    }
    else
    {
        HAL_FLASH_OB_Launch();
    }

    HAL_FLASH_OB_Lock();
    HAL_FLASH_Lock();

    return status;
}
