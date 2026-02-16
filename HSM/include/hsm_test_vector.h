#ifndef HSM_TEST_VECTOR_H
#define HSM_TEST_VECTOR_H

#include <stdint.h>

/* NIST AES-128-CBC Test Vector (SP 800-38A F.2.1) */
#define AES_CBC_KEY_SIZE        16U
#define AES_CBC_IV_SIZE         16U
#define AES_CBC_DATA_SIZE       64U

extern const uint8_t aes_cbc_key[AES_CBC_KEY_SIZE];
extern const uint8_t aes_cbc_iv[AES_CBC_IV_SIZE];
extern const uint8_t aes_cbc_plaintext[AES_CBC_DATA_SIZE];
extern const uint8_t aes_cbc_expected_ciphertext[AES_CBC_DATA_SIZE];

extern uint8_t aes_cbc_computed_ciphertext[AES_CBC_DATA_SIZE];
extern uint8_t aes_cbc_computed_plaintext[AES_CBC_DATA_SIZE];

#endif /* HSM_TEST_VECTOR_H */
