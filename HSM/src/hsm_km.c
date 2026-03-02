#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_flash_ex.h"
#include "hsm_km.h"
#include <string.h>

/* ============================================================
 * Flash Address Map
 * ============================================================ */
#define KM_FLASH_BASE           0x080E0000UL   /* Sector 11 start     */
#define KM_STORE_HDR_ADDR       (KM_FLASH_BASE + 0x0000UL)  /* 16B    */
#define KM_SYM_REGION_ADDR      (KM_FLASH_BASE + 0x0010UL)  /* 1,536B */
#define KM_ECC_REGION_ADDR      (KM_SYM_REGION_ADDR + sizeof(SymKeySlot_t) * KM_SYM_SLOT_COUNT)  /* 1,024B */
#define KM_RSA_REGION_ADDR      (KM_ECC_REGION_ADDR + sizeof(EccKeySlot_t) * KM_ECC_SLOT_COUNT)  /* 2,176B */

#define KM_STORE_MAGIC          0xDEADC0DEUL
#define KM_SLOT_MAGIC           0xAA55CC33UL

/* ============================================================
 * Store Header (16 bytes, written at KM_STORE_HDR_ADDR)
 * ============================================================ */
typedef struct
{
    uint32_t magic;         /* 4B: KM_STORE_MAGIC        */
    uint16_t version;       /* 2B: store format version  */
    uint16_t key_count;     /* 2B: number of active keys */
    uint32_t crc32;         /* 4B: reserved (future use) */
    uint32_t reserved;      /* 4B                        */
} StoreHeader_t;            /* = 16 bytes                */

/* ============================================================
 * CRC32 - IEEE 802.3 polynomial (software, no lookup table)
 * ============================================================
 * KM_CRC32_Update: incremental update, crc_in = 0xFFFFFFFF on first call.
 *                  Returns intermediate state (NOT finalized).
 * KM_CRC32:        single-buffer convenience wrapper.
 * ============================================================ */
static uint32_t KM_CRC32_Update(uint32_t crc_in, const uint8_t *data, uint16_t len)
{
    uint32_t crc = crc_in;
    for (uint16_t i = 0; i < len; i++)
    {
        crc ^= (uint32_t)data[i];
        for (int j = 0; j < 8; j++)
        {
            crc = (crc & 1U) ? ((crc >> 1) ^ 0xEDB88320UL) : (crc >> 1);
        }
    }
    return crc;  /* caller must apply ~crc to finalize */
}

static uint32_t KM_CRC32(const uint8_t *data, uint16_t len)
{
    return ~KM_CRC32_Update(0xFFFFFFFFUL, data, len);
}

/* ============================================================
 * Flash Write Helper
 * ============================================================
 * Writes `len` bytes from `data` to Flash `addr` using WORD
 * (32-bit) programming. `addr` and `len` must be multiples of 4.
 * Flash bits can only transition 1→0 without erase.
 * ============================================================ */
static RetStatus KM_FlashWrite(uint32_t addr, const uint8_t *data, uint16_t len)
{
    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        return RET_NOK;
    }

    for (uint16_t i = 0; i < len; i += 4U)
    {
        uint32_t word;
        memcpy(&word, &data[i], sizeof(word));

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i, word) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return RET_NOK;
        }
    }

    HAL_FLASH_Lock();
    return RET_OK;
}

/* ============================================================
 * Flash Read Helper (memory-mapped direct read)
 * ============================================================ */
static void KM_FlashRead(uint32_t addr, uint8_t *buf, uint16_t len)
{
    memcpy(buf, (const void *)addr, len);
}

/* ============================================================
 * Slot Address Calculator
 * ============================================================ */
static uint32_t KM_GetSymSlotAddr(uint8_t key_id)
{
    return KM_SYM_REGION_ADDR + ((uint32_t)key_id * sizeof(SymKeySlot_t));
}

/* ============================================================
 * KM_Init
 * ============================================================
 * Checks if the key store has been formatted.
 * Returns RET_OK if valid, RET_NOK if unformatted (call KM_Format).
 * ============================================================ */
RetStatus KM_Init(void)
{
    StoreHeader_t hdr;
    KM_FlashRead(KM_STORE_HDR_ADDR, (uint8_t *)&hdr, sizeof(hdr));

    if (hdr.magic != KM_STORE_MAGIC)
    {
        return RET_NOK;
    }
    return RET_OK;
}

/* ============================================================
 * KM_Format
 * ============================================================
 * Erases Flash Sector 11 (128KB) and writes an empty store header.
 * WARNING: destroys ALL stored keys. Use with caution.
 * ============================================================ */
RetStatus KM_Format(void)
{
    uint32_t sector_error = 0;

    FLASH_EraseInitTypeDef erase_init = {
        .TypeErase    = FLASH_TYPEERASE_SECTORS,
        .Sector       = FLASH_SECTOR_11,
        .NbSectors    = 1U,
        .VoltageRange = FLASH_VOLTAGE_RANGE_3,
    };

    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        return RET_NOK;
    }

    if (HAL_FLASHEx_Erase(&erase_init, &sector_error) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return RET_NOK;
    }

    HAL_FLASH_Lock();

    StoreHeader_t hdr;
    memset(&hdr, 0xFF, sizeof(hdr));
    hdr.magic     = KM_STORE_MAGIC;
    hdr.version   = 0x0001U;
    hdr.key_count = 0U;
    hdr.crc32     = 0U;
    hdr.reserved  = 0U;

    return KM_FlashWrite(KM_STORE_HDR_ADDR, (const uint8_t *)&hdr, sizeof(hdr));
}

/* ============================================================
 * KM_WriteSymKey
 * ============================================================
 * Writes a symmetric key to its designated Flash slot.
 *
 * Slot assignment: slot_index == key_id (fixed mapping).
 * Each slot can only be written ONCE after a KM_Format().
 * To overwrite, call KM_DeleteSymKey() first, then KM_Format(),
 * then write again (sector erase is required to reuse a slot).
 *
 * key_data : raw key bytes
 * key_len  : actual key length (must be <= 64)
 * label    : up to 11 ASCII chars, null-terminated
 * ============================================================ */
RetStatus KM_WriteSymKey(uint8_t       key_id,
                         KeyType_t     key_type,
                         uint8_t       key_usage,
                         const uint8_t *key_data,
                         uint16_t      key_len,
                         const char   *label)
{
    if (key_id >= KM_SYM_SLOT_COUNT)          { return RET_NOK; }
    if (key_data == NULL || key_len == 0U)    { return RET_NOK; }
    if (key_len > sizeof(((SymKeySlot_t *)0)->data)) { return RET_NOK; }

    uint32_t slot_addr = KM_GetSymSlotAddr(key_id);

    /* Verify slot is in erased state (never written) */
    KeyHeader_t existing;
    KM_FlashRead(slot_addr, (uint8_t *)&existing, sizeof(existing));
    if (existing.key_status != (uint8_t)KEY_STATUS_EMPTY)
    {
        /* Already written (ACTIVE or REVOKED): requires KM_Format() */
        return RET_NOK;
    }

    /* Build slot in RAM */
    SymKeySlot_t slot;
    memset(&slot, 0x00, sizeof(slot));

    slot.header.magic      = KM_SLOT_MAGIC;
    slot.header.key_id     = key_id;
    slot.header.key_type   = (uint8_t)key_type;
    slot.header.key_usage  = key_usage;
    slot.header.key_status = (uint8_t)KEY_STATUS_ACTIVE;
    slot.header.key_len    = key_len;
    slot.header.flags      = 0x0000U;
    slot.header.version    = 1U;

    memset(slot.header.label, 0x00, sizeof(slot.header.label));
    if (label != NULL)
    {
        strncpy((char *)slot.header.label, label,
                sizeof(slot.header.label) - 1U);
    }

    memcpy(slot.data, key_data, key_len);
    /* Remaining bytes in slot.data are already zero-padded by memset */

    slot.header.crc32 = KM_CRC32(slot.data, sizeof(slot.data));

    return KM_FlashWrite(slot_addr, (const uint8_t *)&slot, sizeof(slot));
}

/* ============================================================
 * KM_ReadSymKey
 * ============================================================
 * Reads key data from the designated slot.
 * Verifies magic, status, and CRC32 before returning data.
 *
 * key_data_out : caller-allocated buffer (>= 64 bytes recommended)
 * key_len_out  : set to actual key length on success
 * ============================================================ */
RetStatus KM_ReadSymKey(uint8_t   key_id,
                        uint8_t  *key_data_out,
                        uint16_t *key_len_out)
{
    if (key_id >= KM_SYM_SLOT_COUNT)              { return RET_NOK; }
    if (key_data_out == NULL || key_len_out == NULL) { return RET_NOK; }

    uint32_t slot_addr = KM_GetSymSlotAddr(key_id);

    SymKeySlot_t slot;
    KM_FlashRead(slot_addr, (uint8_t *)&slot, sizeof(slot));

    if (slot.header.magic != KM_SLOT_MAGIC)
    {
        return RET_NOK;  /* Slot never written */
    }
    if (slot.header.key_status != (uint8_t)KEY_STATUS_ACTIVE)
    {
        return RET_NOK;  /* Key revoked or inactive */
    }

    /* Integrity check */
    uint32_t expected_crc = KM_CRC32(slot.data, sizeof(slot.data));
    if (expected_crc != slot.header.crc32)
    {
        return RET_NOK;  /* Data corrupted */
    }

    memcpy(key_data_out, slot.data, slot.header.key_len);
    *key_len_out = slot.header.key_len;

    return RET_OK;
}

/* ============================================================
 * KM_DeleteSymKey
 * ============================================================
 * Security wipe: zeroes the key data area first, then zeroes
 * the header (marking slot as REVOKED = 0x00).
 *
 * Two-step write order ensures key material is destroyed even
 * if power loss occurs between the two writes.
 *
 * NOTE: Flash bits 1→0 transition without erase is valid.
 *       After deletion, slot cannot be reused until KM_Format().
 * ============================================================ */
RetStatus KM_DeleteSymKey(uint8_t key_id)
{
    if (key_id >= KM_SYM_SLOT_COUNT) { return RET_NOK; }

    uint32_t slot_addr = KM_GetSymSlotAddr(key_id);

    /* Verify slot is currently active */
    KeyHeader_t hdr;
    KM_FlashRead(slot_addr, (uint8_t *)&hdr, sizeof(hdr));

    if (hdr.magic != KM_SLOT_MAGIC ||
        hdr.key_status != (uint8_t)KEY_STATUS_ACTIVE)
    {
        return RET_NOK;
    }

    /* Step 1: Zero-fill key data (security wipe) */
    uint8_t zeros_data[sizeof(((SymKeySlot_t *)0)->data)];
    memset(zeros_data, 0x00, sizeof(zeros_data));

    uint32_t data_addr = slot_addr + sizeof(KeyHeader_t);
    if (KM_FlashWrite(data_addr, zeros_data, sizeof(zeros_data)) != RET_OK)
    {
        return RET_NOK;
    }

    /* Step 2: Zero-fill header (invalidates the slot) */
    uint8_t zeros_hdr[sizeof(KeyHeader_t)];
    memset(zeros_hdr, 0x00, sizeof(zeros_hdr));

    return KM_FlashWrite(slot_addr, zeros_hdr, sizeof(zeros_hdr));
}

/* ============================================================
 * KM_GetKeyInfo
 * ============================================================
 * Reads only the 32-byte header of the specified slot.
 * No key material is returned.
 * ============================================================ */
RetStatus KM_GetKeyInfo(uint8_t key_id, KeyHeader_t *header_out)
{
    if (key_id >= KM_SYM_SLOT_COUNT || header_out == NULL) { return RET_NOK; }

    uint32_t slot_addr = KM_GetSymSlotAddr(key_id);
    KM_FlashRead(slot_addr, (uint8_t *)header_out, sizeof(KeyHeader_t));

    if (header_out->magic != KM_SLOT_MAGIC)
    {
        return RET_NOK;
    }
    return RET_OK;
}

/* ============================================================
 * Phase 2: ECC P-256 Key Management
 * ============================================================ */

static uint32_t KM_GetEccSlotAddr(uint8_t ecc_key_id)
{
    return KM_ECC_REGION_ADDR + ((uint32_t)ecc_key_id * sizeof(EccKeySlot_t));
}

/* ============================================================
 * KM_WriteEccKey
 * ============================================================
 * Writes a P-256 key pair (d, Qx, Qy) to the designated ECC slot.
 * d, Qx, Qy must each point to exactly 32 bytes.
 * ============================================================ */
RetStatus KM_WriteEccKey(uint8_t       ecc_key_id,
                         uint8_t       key_usage,
                         const uint8_t *d,
                         const uint8_t *Qx,
                         const uint8_t *Qy,
                         const char   *label)
{
    if (ecc_key_id >= KM_ECC_SLOT_COUNT)          { return RET_NOK; }
    if (d == NULL || Qx == NULL || Qy == NULL)    { return RET_NOK; }

    uint32_t slot_addr = KM_GetEccSlotAddr(ecc_key_id);

    /* Verify slot is in erased state */
    KeyHeader_t existing;
    KM_FlashRead(slot_addr, (uint8_t *)&existing, sizeof(existing));
    if (existing.key_status != (uint8_t)KEY_STATUS_EMPTY)
    {
        return RET_NOK;
    }

    /* Build slot in RAM */
    EccKeySlot_t slot;
    memset(&slot, 0x00, sizeof(slot));

    slot.header.magic      = KM_SLOT_MAGIC;
    slot.header.key_id     = ecc_key_id;
    slot.header.key_type   = (uint8_t)KEY_TYPE_ECC_P256;
    slot.header.key_usage  = key_usage;
    slot.header.key_status = (uint8_t)KEY_STATUS_ACTIVE;
    slot.header.key_len    = 96U;  /* d(32) + Qx(32) + Qy(32) */
    slot.header.flags      = 0x0000U;
    slot.header.version    = 1U;

    memset(slot.header.label, 0x00, sizeof(slot.header.label));
    if (label != NULL)
    {
        strncpy((char *)slot.header.label, label,
                sizeof(slot.header.label) - 1U);
    }

    memcpy(slot.d,  d,  32U);
    memcpy(slot.Qx, Qx, 32U);
    memcpy(slot.Qy, Qy, 32U);

    /* CRC32 over d||Qx||Qy (96 bytes) */
    uint8_t ecc_data[96];
    memcpy(&ecc_data[0],  slot.d,  32U);
    memcpy(&ecc_data[32], slot.Qx, 32U);
    memcpy(&ecc_data[64], slot.Qy, 32U);
    slot.header.crc32 = KM_CRC32(ecc_data, sizeof(ecc_data));

    return KM_FlashWrite(slot_addr, (const uint8_t *)&slot, sizeof(slot));
}

/* ============================================================
 * KM_ReadEccKey
 * ============================================================
 * Reads ECC key components. Pass NULL for any unneeded output.
 * e.g. pass NULL for d to read only the public key (Qx, Qy).
 * ============================================================ */
RetStatus KM_ReadEccKey(uint8_t  ecc_key_id,
                        uint8_t *d_out,
                        uint8_t *Qx_out,
                        uint8_t *Qy_out)
{
    if (ecc_key_id >= KM_ECC_SLOT_COUNT) { return RET_NOK; }

    uint32_t slot_addr = KM_GetEccSlotAddr(ecc_key_id);

    EccKeySlot_t slot;
    KM_FlashRead(slot_addr, (uint8_t *)&slot, sizeof(slot));

    if (slot.header.magic != KM_SLOT_MAGIC)               { return RET_NOK; }
    if (slot.header.key_status != (uint8_t)KEY_STATUS_ACTIVE) { return RET_NOK; }

    /* Integrity check over d||Qx||Qy */
    uint8_t ecc_data[96];
    memcpy(&ecc_data[0],  slot.d,  32U);
    memcpy(&ecc_data[32], slot.Qx, 32U);
    memcpy(&ecc_data[64], slot.Qy, 32U);
    if (KM_CRC32(ecc_data, sizeof(ecc_data)) != slot.header.crc32) { return RET_NOK; }

    if (d_out  != NULL) { memcpy(d_out,  slot.d,  32U); }
    if (Qx_out != NULL) { memcpy(Qx_out, slot.Qx, 32U); }
    if (Qy_out != NULL) { memcpy(Qy_out, slot.Qy, 32U); }

    return RET_OK;
}

/* ============================================================
 * KM_DeleteEccKey
 * ============================================================ */
RetStatus KM_DeleteEccKey(uint8_t ecc_key_id)
{
    if (ecc_key_id >= KM_ECC_SLOT_COUNT) { return RET_NOK; }

    uint32_t slot_addr = KM_GetEccSlotAddr(ecc_key_id);

    KeyHeader_t hdr;
    KM_FlashRead(slot_addr, (uint8_t *)&hdr, sizeof(hdr));
    if (hdr.magic != KM_SLOT_MAGIC ||
        hdr.key_status != (uint8_t)KEY_STATUS_ACTIVE)
    {
        return RET_NOK;
    }

    /* Step 1: Zero-fill key data (d, Qx, Qy) */
    uint8_t zeros_data[sizeof(EccKeySlot_t) - sizeof(KeyHeader_t)];
    memset(zeros_data, 0x00, sizeof(zeros_data));

    uint32_t data_addr = slot_addr + sizeof(KeyHeader_t);
    if (KM_FlashWrite(data_addr, zeros_data, sizeof(zeros_data)) != RET_OK)
    {
        return RET_NOK;
    }

    /* Step 2: Zero-fill header */
    uint8_t zeros_hdr[sizeof(KeyHeader_t)];
    memset(zeros_hdr, 0x00, sizeof(zeros_hdr));

    return KM_FlashWrite(slot_addr, zeros_hdr, sizeof(zeros_hdr));
}

/* ============================================================
 * Phase 3: RSA-2048 Key Management
 * ============================================================ */

static uint32_t KM_GetRsaSlotAddr(uint8_t rsa_key_id)
{
    return KM_RSA_REGION_ADDR + ((uint32_t)rsa_key_id * sizeof(RsaKeySlot_t));
}

/* ============================================================
 * KM_WriteRsaKey
 * ============================================================
 * Writes an RSA-2048 key (n, d) to the designated RSA slot.
 * n and d must each point to exactly 256 bytes (big-endian).
 * Public exponent e is assumed to be 65537 (hardcoded).
 * ============================================================ */
RetStatus KM_WriteRsaKey(uint8_t       rsa_key_id,
                         uint8_t       key_usage,
                         const uint8_t *n,
                         const uint8_t *d,
                         const char   *label)
{
    if (rsa_key_id >= KM_RSA_SLOT_COUNT) { return RET_NOK; }
    if (n == NULL || d == NULL)          { return RET_NOK; }

    uint32_t slot_addr = KM_GetRsaSlotAddr(rsa_key_id);

    /* Verify slot is in erased state */
    KeyHeader_t existing;
    KM_FlashRead(slot_addr, (uint8_t *)&existing, sizeof(existing));
    if (existing.key_status != (uint8_t)KEY_STATUS_EMPTY)
    {
        return RET_NOK;
    }

    /* Build slot in RAM.
     * static: RsaKeySlot_t is 544B. Declaring on stack risks overflow
     * on the default 1KB stack (bare-metal, no reentrancy needed). */
    static RsaKeySlot_t slot;
    memset(&slot, 0x00, sizeof(slot));

    slot.header.magic      = KM_SLOT_MAGIC;
    slot.header.key_id     = rsa_key_id;
    slot.header.key_type   = (uint8_t)KEY_TYPE_RSA2048;
    slot.header.key_usage  = key_usage;
    slot.header.key_status = (uint8_t)KEY_STATUS_ACTIVE;
    slot.header.key_len    = 512U;  /* n(256) + d(256) */
    slot.header.flags      = 0x0000U;
    slot.header.version    = 1U;

    memset(slot.header.label, 0x00, sizeof(slot.header.label));
    if (label != NULL)
    {
        strncpy((char *)slot.header.label, label,
                sizeof(slot.header.label) - 1U);
    }

    memcpy(slot.n, n, 256U);
    memcpy(slot.d, d, 256U);

    /* CRC32 over n||d: computed incrementally to avoid a 512B stack buffer */
    uint32_t crc = 0xFFFFFFFFUL;
    crc = KM_CRC32_Update(crc, slot.n, 256U);
    crc = KM_CRC32_Update(crc, slot.d, 256U);
    slot.header.crc32 = ~crc;

    return KM_FlashWrite(slot_addr, (const uint8_t *)&slot, sizeof(slot));
}

/* ============================================================
 * KM_ReadRsaKey
 * ============================================================
 * Reads RSA key components.
 * Pass NULL for d to read only the public modulus n.
 * n_out and d_out (if non-NULL) must each be at least 256 bytes.
 * ============================================================ */
RetStatus KM_ReadRsaKey(uint8_t  rsa_key_id,
                        uint8_t *n_out,
                        uint8_t *d_out)
{
    if (rsa_key_id >= KM_RSA_SLOT_COUNT) { return RET_NOK; }
    if (n_out == NULL)                   { return RET_NOK; }

    uint32_t slot_addr = KM_GetRsaSlotAddr(rsa_key_id);

    /* static: same reasoning as KM_WriteRsaKey (544B, bare-metal) */
    static RsaKeySlot_t slot;
    KM_FlashRead(slot_addr, (uint8_t *)&slot, sizeof(slot));

    if (slot.header.magic != KM_SLOT_MAGIC)                    { return RET_NOK; }
    if (slot.header.key_status != (uint8_t)KEY_STATUS_ACTIVE)  { return RET_NOK; }

    /* Integrity check over n||d: incremental CRC, no 512B stack buffer */
    uint32_t crc = 0xFFFFFFFFUL;
    crc = KM_CRC32_Update(crc, slot.n, 256U);
    crc = KM_CRC32_Update(crc, slot.d, 256U);
    if (~crc != slot.header.crc32) { return RET_NOK; }

    memcpy(n_out, slot.n, 256U);
    if (d_out != NULL) { memcpy(d_out, slot.d, 256U); }

    return RET_OK;
}

/* ============================================================
 * KM_DeleteRsaKey
 * ============================================================ */
RetStatus KM_DeleteRsaKey(uint8_t rsa_key_id)
{
    if (rsa_key_id >= KM_RSA_SLOT_COUNT) { return RET_NOK; }

    uint32_t slot_addr = KM_GetRsaSlotAddr(rsa_key_id);

    KeyHeader_t hdr;
    KM_FlashRead(slot_addr, (uint8_t *)&hdr, sizeof(hdr));
    if (hdr.magic != KM_SLOT_MAGIC ||
        hdr.key_status != (uint8_t)KEY_STATUS_ACTIVE)
    {
        return RET_NOK;
    }

    /* Step 1: Zero-fill key data (n, d) */
    uint8_t zeros_data[sizeof(RsaKeySlot_t) - sizeof(KeyHeader_t)];
    memset(zeros_data, 0x00, sizeof(zeros_data));

    uint32_t data_addr = slot_addr + sizeof(KeyHeader_t);
    if (KM_FlashWrite(data_addr, zeros_data, sizeof(zeros_data)) != RET_OK)
    {
        return RET_NOK;
    }

    /* Step 2: Zero-fill header */
    uint8_t zeros_hdr[sizeof(KeyHeader_t)];
    memset(zeros_hdr, 0x00, sizeof(zeros_hdr));

    return KM_FlashWrite(slot_addr, zeros_hdr, sizeof(zeros_hdr));
}
