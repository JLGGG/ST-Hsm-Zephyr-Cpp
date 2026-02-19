#ifndef HSM_KM_H
#define HSM_KM_H

#include "hsm_type.h"
#include <stdint.h>

/* ============================================================
 * Flash Sector 11 Key Storage Layout (128KB = 131,072 bytes)
 * ============================================================
 *
 *  0x080E0000 +---------------------------------------------+
 *             | Store Header                     (16 bytes) |
 *             |  magic     : 0xDEADC0DE  (4B)               |
 *             |  version   : uint16_t    (2B)               |
 *             |  key_count : uint16_t    (2B)               |
 *             |  crc32     : uint32_t    (4B)               |
 *             |  reserved  : uint32_t    (4B)               |
 *             +---------------------------------------------+
 *             | Symmetric Key Region           (1,536 bytes)|
 *             |  16 slots x 96 bytes/slot                   |
 *             |  [ KeyHeader(32B) | KeyData(64B) ]          |
 *             |                                             |
 *             |  Slot  0 : DRK     AES-256      32B         |
 *             |            Device Root Key                  |
 *             |            usage: DERIVE                    |
 *             |  Slot  1 : KEK     AES-256      32B         |
 *             |            Key Encryption Key               |
 *             |            usage: WRAP                      |
 *             |  Slot  2 : DEK_0   AES-256      32B         |
 *             |  Slot  3 : DEK_1   AES-256      32B         |
 *             |  Slot  4 : DEK_2   AES-256      32B         |
 *             |  Slot  5 : DEK_3   AES-256      32B         |
 *             |            Data Encryption Keys             |
 *             |            usage: ENCRYPT | DECRYPT         |
 *             |  Slot  6 : SK_0    AES-128      16B         |
 *             |  Slot  7 : SK_1    AES-128      16B         |
 *             |  Slot  8 : SK_2    AES-128      16B         |
 *             |  Slot  9 : SK_3    AES-128      16B         |
 *             |            Session Keys (short-lived)       |
 *             |            usage: ENCRYPT | DECRYPT         |
 *             |  Slot 10 : MK_0    HMAC-SHA256  32B         |
 *             |  Slot 11 : MK_1    HMAC-SHA256  32B         |
 *             |            MAC Keys                         |
 *             |            usage: MAC                       |
 *             |  Slot 12 : FL_ENC  AES-256      32B         |
 *             |            FL Model Encryption Key          |
 *             |            usage: ENCRYPT | DECRYPT         |
 *             |  Slot 13 : FL_MK   HMAC-SHA256  32B         |
 *             |            FL Update Verification Key       |
 *             |            usage: MAC                       |
 *             |  Slot 14 : Reserved                         |
 *             |  Slot 15 : Reserved                         |
 *             +---------------------------------------------+
 *             | ECC Key Region                 (1,024 bytes)|
 *             |  8 slots x 128 bytes/slot                   |
 *             |  [ KeyHeader(32B) | KeyData(96B) ]          |
 *             |                                             |
 *             |  Slot 0 : ECDSA_DEV  P-256 pair  96B        |
 *             |           Device Identity/Signing Key       |
 *             |           usage: SIGN | VERIFY              |
 *             |  Slot 1 : ECDH_0     P-256 pair  96B        |
 *             |  Slot 2 : ECDH_1     P-256 pair  96B        |
 *             |           Key Exchange Keys                 |
 *             |           usage: DERIVE                     |
 *             |  Slot 3~7 : Reserved                        |
 *             +---------------------------------------------+
 *             | RSA Key Region                 (2,176 bytes)|
 *             |  4 slots x 544 bytes/slot                   |
 *             |  [ KeyHeader(32B) | KeyData(512B) ]         |
 *             |                                             |
 *             |  Slot 0 : RSA_SIGN  RSA-2048  512B          |
 *             |           Certificate Signing Key           |
 *             |           usage: SIGN | VERIFY              |
 *             |  Slot 1~3 : Reserved                        |
 *             +---------------------------------------------+
 *             | Reserved                    (~126,320 bytes)|
 *             +---------------------------------------------+
 *  0x080FFFFF
 *
 *  Total used : 16 + 1,536 + 1,024 + 2,176 = 4,752 bytes (3.6%)
 */

/* ============================================================
 * Symmetric Key Slot IDs (= slot index in Symmetric Region)
 * ============================================================ */
#define KM_KEY_ID_DRK       0x00U   /* Device Root Key     - AES-256  */
#define KM_KEY_ID_KEK       0x01U   /* Key Encryption Key  - AES-256  */
#define KM_KEY_ID_DEK_0     0x02U   /* Data Enc Key 0      - AES-256  */
#define KM_KEY_ID_DEK_1     0x03U   /* Data Enc Key 1      - AES-256  */
#define KM_KEY_ID_DEK_2     0x04U   /* Data Enc Key 2      - AES-256  */
#define KM_KEY_ID_DEK_3     0x05U   /* Data Enc Key 3      - AES-256  */
#define KM_KEY_ID_SK_0      0x06U   /* Session Key 0       - AES-128  */
#define KM_KEY_ID_SK_1      0x07U   /* Session Key 1       - AES-128  */
#define KM_KEY_ID_SK_2      0x08U   /* Session Key 2       - AES-128  */
#define KM_KEY_ID_SK_3      0x09U   /* Session Key 3       - AES-128  */
#define KM_KEY_ID_MK_0      0x0AU   /* MAC Key 0           - HMAC-SHA256 */
#define KM_KEY_ID_MK_1      0x0BU   /* MAC Key 1           - HMAC-SHA256 */
#define KM_KEY_ID_FL_ENC    0x0CU   /* FL Model Enc Key    - AES-256  */
#define KM_KEY_ID_FL_MK     0x0DU   /* FL MAC Key          - HMAC-SHA256 */
#define KM_SYM_SLOT_COUNT   16U

/* ============================================================
 * Key Types
 * ============================================================ */
typedef enum
{
    KEY_TYPE_AES128       = 0x01U,
    KEY_TYPE_AES192       = 0x02U,
    KEY_TYPE_AES256       = 0x03U,
    KEY_TYPE_HMAC_SHA256  = 0x04U,
    KEY_TYPE_HMAC_SHA512  = 0x05U,
    KEY_TYPE_ECC_P256     = 0x10U,
    KEY_TYPE_RSA2048      = 0x20U,
} KeyType_t;

/* ============================================================
 * Key Usage Flags (bitmask)
 * ============================================================ */
#define KEY_USAGE_ENCRYPT   0x01U
#define KEY_USAGE_DECRYPT   0x02U
#define KEY_USAGE_SIGN      0x04U
#define KEY_USAGE_VERIFY    0x08U
#define KEY_USAGE_WRAP      0x10U
#define KEY_USAGE_DERIVE    0x20U
#define KEY_USAGE_MAC       0x40U

/* ============================================================
 * Key Status
 * ============================================================ */
typedef enum
{
    KEY_STATUS_EMPTY    = 0xFFU,  /* Flash erased state (never written) */
    KEY_STATUS_ACTIVE   = 0x01U,  /* Valid and usable                  */
    KEY_STATUS_INACTIVE = 0x02U,  /* Temporarily disabled              */
    KEY_STATUS_REVOKED  = 0x00U,  /* Zeroed out; slot unusable         */
} KeyStatus_t;

/* ============================================================
 * Key Header (32 bytes, naturally aligned)
 * ============================================================
 *  offset  0 : magic      (4B) - 0xAA55CC33
 *  offset  4 : key_id     (1B)
 *  offset  5 : key_type   (1B) - KeyType_t
 *  offset  6 : key_usage  (1B) - usage flags bitmask
 *  offset  7 : key_status (1B) - KeyStatus_t
 *  offset  8 : key_len    (2B) - actual key data length in bytes
 *  offset 10 : flags      (2B) - reserved for future use
 *  offset 12 : version    (4B) - monotonic counter
 *  offset 16 : label     (12B) - null-terminated ASCII label
 *  offset 28 : crc32      (4B) - CRC32 over slot data area
 *                                (size varies by key type)
 * ============================================================ */
typedef struct
{
    uint32_t magic;         /* 4B  */
    uint8_t  key_id;        /* 1B  */
    uint8_t  key_type;      /* 1B  */
    uint8_t  key_usage;     /* 1B  */
    uint8_t  key_status;    /* 1B  */
    uint16_t key_len;       /* 2B  */
    uint16_t flags;         /* 2B  */
    uint32_t version;       /* 4B  */
    uint8_t  label[12];     /* 12B */
    uint32_t crc32;         /* 4B  */
} KeyHeader_t;              /* = 32 bytes */

/* ============================================================
 * Symmetric Key Slot (96 bytes)
 * ============================================================
 *  [ KeyHeader(32B) | KeyData(64B) ]
 *  KeyData is zero-padded to 64B regardless of actual key_len
 * ============================================================ */
typedef struct
{
    KeyHeader_t header;     /* 32B */
    uint8_t     data[64];   /* 64B */
} SymKeySlot_t;             /* = 96 bytes */

/* ============================================================
 * Public API - Phase 1: Symmetric Key Management
 * ============================================================ */

/* Initialize: verify store header. Returns RET_NOK if not formatted. */
RetStatus KM_Init(void);

/* Format: erase Sector 11 and write fresh store header.
 * WARNING: destroys ALL stored keys. */
RetStatus KM_Format(void);

/* Write a symmetric key to the designated slot.
 * Returns RET_NOK if slot already used (requires KM_Format to reuse). */
RetStatus KM_WriteSymKey(uint8_t      key_id,
                         KeyType_t    key_type,
                         uint8_t      key_usage,
                         const uint8_t *key_data,
                         uint16_t     key_len,
                         const char  *label);

/* Read key data from slot. key_data_out must be at least 64 bytes. */
RetStatus KM_ReadSymKey(uint8_t   key_id,
                        uint8_t  *key_data_out,
                        uint16_t *key_len_out);

/* Zero-fill slot data and revoke key.
 * NOTE: slot cannot be reused until KM_Format() is called. */
RetStatus KM_DeleteSymKey(uint8_t key_id);

/* Read key header only (no key material returned). */
RetStatus KM_GetKeyInfo(uint8_t key_id, KeyHeader_t *header_out);

/* ============================================================
 * ECC Key Slot IDs (= slot index in ECC Region)
 * ============================================================ */
#define KM_ECC_KEY_ID_ECDSA_DEV  0x00U   /* Device Identity/Signing - P-256 pair */
#define KM_ECC_KEY_ID_ECDH_0     0x01U   /* Key Exchange 0          - P-256 pair */
#define KM_ECC_KEY_ID_ECDH_1     0x02U   /* Key Exchange 1          - P-256 pair */
#define KM_ECC_SLOT_COUNT        8U

/* ============================================================
 * ECC Key Slot (128 bytes)
 * ============================================================
 *  [ KeyHeader(32B) | d(32B) | Qx(32B) | Qy(32B) ]
 *
 *  d  : private key scalar (32B = 256-bit)
 *  Qx : public key x coordinate (32B)
 *  Qy : public key y coordinate (32B)
 *
 *  CRC32 is computed over d||Qx||Qy (96 bytes)
 * ============================================================ */
typedef struct
{
    KeyHeader_t header;     /* 32B */
    uint8_t     d[32];      /* 32B - private key scalar      */
    uint8_t     Qx[32];     /* 32B - public key x coordinate */
    uint8_t     Qy[32];     /* 32B - public key y coordinate */
} EccKeySlot_t;             /* = 128 bytes */

/* ============================================================
 * Public API - Phase 2: ECC P-256 Key Management
 * ============================================================ */

/* Write an ECC P-256 key pair.
 * d, Qx, Qy must each be exactly 32 bytes.
 * Returns RET_NOK if slot already used (requires KM_Format to reuse). */
RetStatus KM_WriteEccKey(uint8_t       ecc_key_id,
                         uint8_t       key_usage,
                         const uint8_t *d,
                         const uint8_t *Qx,
                         const uint8_t *Qy,
                         const char   *label);

/* Read ECC key components. Each output buffer must be at least 32 bytes.
 * Pass NULL for any component you do not need (e.g. d for public-only read). */
RetStatus KM_ReadEccKey(uint8_t  ecc_key_id,
                        uint8_t *d_out,
                        uint8_t *Qx_out,
                        uint8_t *Qy_out);

/* Zero-fill ECC slot data and revoke key.
 * NOTE: slot cannot be reused until KM_Format() is called. */
RetStatus KM_DeleteEccKey(uint8_t ecc_key_id);

/* ============================================================
 * RSA Key Slot IDs (= slot index in RSA Region)
 * ============================================================ */
#define KM_RSA_KEY_ID_RSA_SIGN   0x00U   /* Certificate Signing - RSA-2048 */
#define KM_RSA_SLOT_COUNT        4U

/* ============================================================
 * RSA Key Slot (544 bytes)
 * ============================================================
 *  [ KeyHeader(32B) | n(256B) | d(256B) ]
 *
 *  n : modulus (256B = 2048-bit, shared by public and private)
 *  d : private exponent (256B = 2048-bit)
 *  e : public exponent 65537 (fixed value)
 *
 *  CRC32 is computed over n||d (512 bytes)
 * ============================================================ */
typedef struct
{
    KeyHeader_t header;     /* 32B  */
    uint8_t     n[256];     /* 256B - modulus           */
    uint8_t     d[256];     /* 256B - private exponent  */
} RsaKeySlot_t;             /* = 544 bytes */

/* ============================================================
 * Public API - Phase 3: RSA-2048 Key Management
 * ============================================================ */

/* Write an RSA-2048 key.
 * n and d must each be exactly 256 bytes (big-endian).
 * Returns RET_NOK if slot already used (requires KM_Format to reuse). */
RetStatus KM_WriteRsaKey(uint8_t       rsa_key_id,
                         uint8_t       key_usage,
                         const uint8_t *n,
                         const uint8_t *d,
                         const char   *label);

/* Read RSA key components. Each output buffer must be at least 256 bytes.
 * Pass NULL for d to read only the public modulus n. */
RetStatus KM_ReadRsaKey(uint8_t  rsa_key_id,
                        uint8_t *n_out,
                        uint8_t *d_out);

/* Zero-fill RSA slot data and revoke key.
 * NOTE: slot cannot be reused until KM_Format() is called. */
RetStatus KM_DeleteRsaKey(uint8_t rsa_key_id);

#endif /* HSM_KM_H */
