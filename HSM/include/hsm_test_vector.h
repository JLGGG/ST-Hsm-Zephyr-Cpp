#ifndef HSM_TEST_VECTOR_H
#define HSM_TEST_VECTOR_H

#include <stdint.h>

#define AES_CBC_KEY_SIZE        16U
#define AES_CBC_IV_SIZE         16U
#define AES_CBC_DATA_SIZE       64U
#define KM_TEST_SYM_KEY_SIZE    32U
#define KM_TEST_ECC_COMP_SIZE   32U
#define KM_TEST_RSA_COMP_SIZE   256U

extern const uint8_t aes_cbc_key[AES_CBC_KEY_SIZE];
extern const uint8_t aes_cbc_iv[AES_CBC_IV_SIZE];
extern const uint8_t aes_cbc_plaintext[AES_CBC_DATA_SIZE];
extern const uint8_t aes_cbc_expected_ciphertext[AES_CBC_DATA_SIZE];
extern uint8_t aes_cbc_computed_ciphertext[AES_CBC_DATA_SIZE];
extern uint8_t aes_cbc_computed_plaintext[AES_CBC_DATA_SIZE];

/* Symmetric: AES-256 key (32 bytes) */
extern const uint8_t km_test_sym_key[KM_TEST_SYM_KEY_SIZE];

/* ECC P-256 key pair: d, Qx, Qy (32 bytes) */
extern const uint8_t km_test_ecc_d[KM_TEST_ECC_COMP_SIZE];
extern const uint8_t km_test_ecc_Qx[KM_TEST_ECC_COMP_SIZE];
extern const uint8_t km_test_ecc_Qy[KM_TEST_ECC_COMP_SIZE];

/* RSA-2048 key: n, d (256 bytes) */
extern const uint8_t km_test_rsa_n[KM_TEST_RSA_COMP_SIZE];
extern const uint8_t km_test_rsa_d[KM_TEST_RSA_COMP_SIZE];

#endif /* HSM_TEST_VECTOR_H */
