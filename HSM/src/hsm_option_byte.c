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
    ob_init.OptionType = OPTIONBYTE_RDP;         // Indicates RDP configuration
    ob_init.RDPLevel   = OB_RDP_LEVEL_1;         // LEVEL_1: Enable readout protection
                                                // LEVEL_0: Disable protection
                                                // LEVEL_2: Permanent protection (Warning!)

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
    ob_init.OptionType = OPTIONBYTE_RDP;         // Indicates RDP configuration 
    ob_init.RDPLevel   = OB_RDP_LEVEL_2;         // LEVEL_2: Permanent protection (Warning!)
                                                // LEVEL_0: Disable protection
                                                // LEVEL_1: Enable readout protection

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

RetStatus setBORLevelThree()
{
    FLASH_OBProgramInitTypeDef ob_init;

    /* Unlock FLASH & Option Bytes */
    HAL_FLASH_Unlock();
    HAL_FLASH_OB_Unlock();

    /* Setup BOR level configuration */
    ob_init.OptionType = OPTIONBYTE_BOR;
    ob_init.BORLevel = OB_BOR_LEVEL3; // Supply voltage ranges from 2.70 to 3.60 V

    /* Launch Option Bytes configuration */
    HAL_FLASHEx_OBProgram(&ob_init);
    HAL_FLASH_OB_Launch();

    /* Lock FLASH & Option Bytes */
    HAL_FLASH_OB_Lock();
    HAL_FLASH_Lock();

    return RET_OK;
}
