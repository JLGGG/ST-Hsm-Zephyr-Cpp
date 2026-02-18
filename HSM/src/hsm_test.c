#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_flash_ex.h"
#include "cmox_crypto.h"
#include "hsm_test.h"
#include "hsm_test_vector.h"
#include "hsm_option_byte.h"

#include <stdio.h>
#include <string.h>

/* WRP Test: Sector 3 (0x0800C000, 16KB, unused by firmware)
 * nWRP[3]=0: protected, nWRP[3]=1: unprotected (RM0090 p.107-108)
 * WRP violation sets WRPERR in FLASH_SR, write is silently ignored */
#define WRP_TEST_SECTOR_ADDRESS  0x0800C000U
#define WRP_TEST_DATA            0xDEADBEEFU
#define WRP_TEST_SECTOR_OB       OB_WRP_SECTOR_3
#define WRP_TEST_SECTOR_NUM      FLASH_SECTOR_3

/* State Machine: OB changes require reset to reload FLASH_OPTCR shadow register.
 * Phase is preserved across resets via RTC Backup Register (BKP0R).
 *   Boot 1 (Phase 0): Enable WRP -> reset
 *   Boot 2 (Phase 1): Verify write blocked -> disable WRP -> reset
 *   Boot 3 (Phase 2): Verify write allowed -> cleanup -> done
 *   Boot 4+: Skip (call hsm_test_wrp_clear() to re-run) */

/* State encoding: upper 24 bits = magic, lower 8 bits = phase */
#define WRP_TEST_MAGIC       0x57525000UL
#define WRP_TEST_PHASE_MASK  0x000000FFUL

#define WRP_PHASE_ENABLE      0x00U
#define WRP_PHASE_VERIFY_ON   0x01U
#define WRP_PHASE_VERIFY_OFF  0x02U
#define WRP_PHASE_DONE        0xFFU

#define WRP_MAKE_STATE(phase) (WRP_TEST_MAGIC | ((phase) & WRP_TEST_PHASE_MASK))
#define WRP_IS_VALID(state)   (((state) & ~WRP_TEST_PHASE_MASK) == WRP_TEST_MAGIC)
#define WRP_GET_PHASE(state)  ((state) & WRP_TEST_PHASE_MASK)


/* RTC Backup Register: retained across software reset, requires PWR clock + DBP */

static void wrp_bkp_init(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
}

static void wrp_bkp_write(uint32_t value)
{
    RTC->BKP0R = value;
}

static uint32_t wrp_bkp_read(void)
{
    return RTC->BKP0R;
}


/* Check WRP status: nWRP bit=0 means protected (RM0090 p.108) */
static uint8_t wrp_is_sector_protected(uint32_t wrp_sector_ob)
{
    FLASH_OBProgramInitTypeDef ob_cfg = {0};
    HAL_FLASHEx_OBGetConfig(&ob_cfg);

    return ((ob_cfg.WRPSector & wrp_sector_ob) == 0U) ? 1U : 0U;
}


/* Erase test sector. VOLTAGE_RANGE_3: 2.7~3.6V, x32 parallelism */
static RetStatus wrp_erase_test_sector(void)
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


/* Phase 0: Enable WRP, save next phase, reset. Does not return on success. */
static void wrp_phase_enable(void)
{
    uint32_t result;

    printf("[WRP] Phase 0: Enabling WRP on Sector 3\r\n");

    result = enableWRP(WRP_TEST_SECTOR_OB);
    if (result != HAL_FLASH_ERROR_NONE)
    {
        printf("[WRP] FAIL - enableWRP error: 0x%08lx\r\n", result);
        wrp_bkp_write(0);
        return;
    }

    printf("[WRP] WRP programmed. Resetting to apply...\r\n");
    wrp_bkp_write(WRP_MAKE_STATE(WRP_PHASE_VERIFY_ON));
    NVIC_SystemReset();
}

/* Phase 1: Verify WRP blocks write, then disable WRP and reset. */
static RetStatus wrp_phase_verify_on(void)
{
    uint32_t result;

    printf("[WRP] Phase 1: Verifying write protection\r\n");

    if (!wrp_is_sector_protected(WRP_TEST_SECTOR_OB))
    {
        printf("[WRP] FAIL - Sector 3 not protected after reset\r\n");
        wrp_bkp_write(0);
        return RET_NOK;
    }
    printf("[WRP] Sector 3 WRP confirmed active (nWRP[3]=0)\r\n");

    /* Clear stale WRPERR before test write */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_WRPERR);

    HAL_FLASH_Unlock();
    result = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                               WRP_TEST_SECTOR_ADDRESS,
                               WRP_TEST_DATA);
    HAL_FLASH_Lock();

    if (result != HAL_OK && __HAL_FLASH_GET_FLAG(FLASH_FLAG_WRPERR))
    {
        printf("[WRP] Write blocked with WRPERR: PASS\r\n");
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_WRPERR);
    }
    else if (result != HAL_OK)
    {
        printf("[WRP] Write failed but WRPERR not set (unexpected)\r\n");
        printf("[WRP] FLASH_SR: 0x%08lx\r\n", FLASH->SR);
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_WRPERR);
    }
    else
    {
        printf("[WRP] FAIL - Write succeeded despite WRP active\r\n");
        wrp_bkp_write(0);
        return RET_NOK;
    }

    printf("[WRP] Disabling WRP on Sector 3\r\n");
    result = disableWRP(WRP_TEST_SECTOR_OB);
    if (result != HAL_FLASH_ERROR_NONE)
    {
        printf("[WRP] FAIL - disableWRP error: 0x%08lx\r\n", result);
        wrp_bkp_write(0);
        return RET_NOK;
    }

    printf("[WRP] WRP disable programmed. Resetting to apply...\r\n");
    wrp_bkp_write(WRP_MAKE_STATE(WRP_PHASE_VERIFY_OFF));
    NVIC_SystemReset();

    return RET_OK; /* unreachable */
}

/* Phase 2: Verify write succeeds after WRP disabled, cleanup, mark done. */
static RetStatus wrp_phase_verify_off(void)
{
    uint32_t result;

    printf("[WRP] Phase 2: Verifying write after WRP disabled\r\n");

    if (wrp_is_sector_protected(WRP_TEST_SECTOR_OB))
    {
        printf("[WRP] FAIL - Sector 3 still protected after reset\r\n");
        wrp_bkp_write(0);
        return RET_NOK;
    }
    printf("[WRP] Sector 3 WRP confirmed inactive (nWRP[3]=1)\r\n");

    HAL_FLASH_Unlock();
    result = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                               WRP_TEST_SECTOR_ADDRESS,
                               WRP_TEST_DATA);
    HAL_FLASH_Lock();

    if (result == HAL_OK)
    {
        printf("[WRP] Write after WRP disabled: PASS\r\n");
    }
    else
    {
        printf("[WRP] FAIL - Write after WRP disabled failed\r\n");
        printf("[WRP] FLASH_SR: 0x%08lx\r\n", FLASH->SR);
        wrp_bkp_write(0);
        return RET_NOK;
    }

    if (wrp_erase_test_sector() != RET_OK)
    {
        printf("[WRP] WARNING - Sector erase cleanup failed\r\n");
        wrp_bkp_write(0);
        return RET_NOK;
    }
    printf("[WRP] Sector 3 cleanup: OK\r\n");

    wrp_bkp_write(WRP_MAKE_STATE(WRP_PHASE_DONE));

    printf("[WRP] === WRP Test Completed: ALL PASS ===\r\n");
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

/* Reset BKP0R to allow WRP test re-execution */
void hsm_test_wrp_clear(void)
{
    wrp_bkp_init();
    wrp_bkp_write(0);
    printf("[WRP] Test state cleared. Will re-run on next call.\r\n");
}

/* WRP integration test: 3 boot cycles, state tracked via BKP0R.
 * Phase 0/1 trigger NVIC_SystemReset() and do NOT return to caller. */
RetStatus hsm_test_wrp(void)
{
    uint32_t state;
    uint32_t phase;

    wrp_bkp_init();
    state = wrp_bkp_read();

    if (WRP_IS_VALID(state))
    {
        phase = WRP_GET_PHASE(state);
    }
    else
    {
        phase = WRP_PHASE_ENABLE;
    }

    printf("\r\n[WRP] === WRP Test (Phase %lu) ===\r\n", phase);

    switch (phase)
    {
    case WRP_PHASE_ENABLE:
        wrp_phase_enable();
        return RET_NOK;

    case WRP_PHASE_VERIFY_ON:
        return wrp_phase_verify_on();

    case WRP_PHASE_VERIFY_OFF:
        return wrp_phase_verify_off();

    case WRP_PHASE_DONE:
        printf("[WRP] Test already completed. Skipping.\r\n");
        printf("[WRP] Call hsm_test_wrp_clear() to re-run.\r\n");
        return RET_OK;

    default:
        printf("[WRP] ERROR - Unknown phase %lu, resetting state\r\n", phase);
        wrp_bkp_write(0);
        return RET_NOK;
    }
}
