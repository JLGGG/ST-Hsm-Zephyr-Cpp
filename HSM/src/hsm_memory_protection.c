#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_flash_ex.h"
#include "hsm_memory_protection.h"

RetStatus setRDPLevelOne()
{
    FLASH_OBProgramInitTypeDef OBInit;

    /* Unlock FLASH & Option Bytes */
    HAL_FLASH_Unlock();
    HAL_FLASH_OB_Unlock();

    /* Initialize RDP configuration */
    OBInit.OptionType = OPTIONBYTE_RDP;         // Indicates RDP configuration
    OBInit.RDPLevel   = OB_RDP_LEVEL_1;         // LEVEL_1: Enable readout protection
                                                // LEVEL_0: Disable protection
                                                // LEVEL_2: Permanent protection (Warning!)

    /* Apply configuration */
    HAL_FLASHEx_OBProgram(&OBInit);

    /* Launch Option Bytes configuration */
    HAL_FLASH_OB_Launch();

    /* Lock FLASH & Option Bytes */
    HAL_FLASH_OB_Lock();
    HAL_FLASH_Lock();
}

#if defined(DISABLE_RDP_LEVEL_TWO)
RetStatus setRDPLevelTwo()
{
    FLASH_OBProgramInitTypeDef OBInit;

    /* Unlock FLASH & Option Bytes */
    HAL_FLASH_Unlock();
    HAL_FLASH_OB_Unlock();

    /* Initialize RDP configuration */
    OBInit.OptionType = OPTIONBYTE_RDP;         // Indicates RDP configuration 
    OBInit.RDPLevel   = OB_RDP_LEVEL_2;         // LEVEL_2: Permanent protection (Warning!)
                                                // LEVEL_0: Disable protection
                                                // LEVEL_1: Enable readout protection

    /* Apply configuration */
    HAL_FLASHEx_OBProgram(&OBInit);

    /* Launch Option Bytes configuration */
    HAL_FLASH_OB_Launch();

    /* Lock FLASH & Option Bytes */
    HAL_FLASH_OB_Lock();
    HAL_FLASH_Lock();
}
#endif
