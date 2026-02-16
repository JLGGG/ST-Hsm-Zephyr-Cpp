#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_flash_ex.h"
#include "cmox_crypto.h"
#include "hsm_test.h"
#include "hsm_test_vector.h"
#include "hsm_option_byte.h"

#include <stdio.h>
#include <string.h>

#define WRP_TEST_SECTOR_ADDRESS  0x0800C000U
#define WRP_TEST_DATA            0xDEADBEEFU
#define WRP_TEST_SECTOR_OB       OB_WRP_SECTOR_3
#define WRP_TEST_SECTOR_NUM      FLASH_SECTOR_3

static RetStatus hsm_test_wrp_erase_sector(void)
{
    FLASH_EraseInitTypeDef erase_init = {0};
    uint32_t sector_error = 0;

    erase_init.TypeErase    = FLASH_TYPEERASE_SECTORS;
    erase_init.Sector       = WRP_TEST_SECTOR_NUM;
    erase_init.NbSectors    = 1;
    erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    HAL_FLASH_Unlock();

    if (HAL_FLASHEx_Erase(&erase_init, &sector_error) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return RET_NOK;
    }

    HAL_FLASH_Lock();
    return RET_OK;
}

RetStatus hsm_test_aes_cbc(void)
{
    cmox_cipher_retval_t ret_val;
    size_t computed_size;
    cmox_init_arg_t init_target = {CMOX_INIT_TARGET_AUTO, NULL};

    if (cmox_initialize(&init_target) != CMOX_INIT_SUCCESS)
    {
        printf("\r\n[AES-CBC] CMOX init failed\r\n");
        return RET_NOK;
    }

    ret_val = cmox_cipher_encrypt(CMOX_AES_CBC_ENC_ALGO,
                                  aes_cbc_plaintext, sizeof(aes_cbc_plaintext),
                                  aes_cbc_key, sizeof(aes_cbc_key),
                                  aes_cbc_iv, sizeof(aes_cbc_iv),
                                  aes_cbc_computed_ciphertext, &computed_size);

    if (ret_val != CMOX_CIPHER_SUCCESS)
    {
        printf("\r\n[AES-CBC] Encryption failed (ret=0x%08x)\r\n", (unsigned int)ret_val);
        return RET_NOK;
    }

    if (computed_size != sizeof(aes_cbc_expected_ciphertext))
    {
        printf("\r\n[AES-CBC] Size mismatch (expected=%u, got=%u)\r\n",
               (unsigned int)sizeof(aes_cbc_expected_ciphertext),
               (unsigned int)computed_size);
        return RET_NOK;
    }

    if (memcmp(aes_cbc_expected_ciphertext, aes_cbc_computed_ciphertext, computed_size) != 0)
    {
        printf("\r\n[AES-CBC] Ciphertext mismatch\r\n");
        return RET_NOK;
    }

    printf("\r\n[AES-CBC] Encryption: OK (size=%u)\r\n", (unsigned int)computed_size);
    return RET_OK;
}

RetStatus hsm_test_wrp(void)
{
    uint32_t result;

    printf("\r\n[WRP] Starting WRP test\r\n");

    /* 1. Enable WRP on test sector */
    result = enableWRP(WRP_TEST_SECTOR_OB);
    if (result != HAL_FLASH_ERROR_NONE)
    {
        printf("[WRP] Failed to enable WRP: 0x%08lx\r\n", result);
        return RET_NOK;
    }
    printf("[WRP] WRP enabled on sector 3\r\n");

    /* 2. Attempt write to protected sector - should fail */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_WRPERR);

    HAL_FLASH_Unlock();
    result = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, WRP_TEST_SECTOR_ADDRESS, WRP_TEST_DATA);
    HAL_FLASH_Lock();

    if (result != HAL_OK && __HAL_FLASH_GET_FLAG(FLASH_FLAG_WRPERR))
    {
        printf("[WRP] Write blocked by WRP (WRPERR set): PASS\r\n");
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_WRPERR);
    }
    else if (result != HAL_OK)
    {
        printf("[WRP] Write failed but WRPERR not set (unexpected)\r\n");
    }
    else
    {
        printf("[WRP] Write succeeded despite WRP: FAIL\r\n");
        return RET_NOK;
    }

    /* 3. Disable WRP */
    result = disableWRP(WRP_TEST_SECTOR_OB);
    if (result != HAL_FLASH_ERROR_NONE)
    {
        printf("[WRP] Failed to disable WRP: 0x%08lx\r\n", result);
        return RET_NOK;
    }
    printf("[WRP] WRP disabled on sector 3\r\n");

    /* 4. Attempt write after WRP disabled - should succeed */
    HAL_FLASH_Unlock();
    result = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, WRP_TEST_SECTOR_ADDRESS, WRP_TEST_DATA);
    HAL_FLASH_Lock();

    if (result == HAL_OK)
    {
        printf("[WRP] Write after disable: PASS\r\n");
    }
    else
    {
        printf("[WRP] Write after disable failed: FAIL\r\n");
        return RET_NOK;
    }

    /* 5. Clean up - erase test sector to restore original state */
    if (hsm_test_wrp_erase_sector() != RET_OK)
    {
        printf("[WRP] Sector erase cleanup failed\r\n");
        return RET_NOK;
    }
    printf("[WRP] Sector cleanup: OK\r\n");

    printf("[WRP] WRP test completed: ALL PASS\r\n");
    return RET_OK;
}
