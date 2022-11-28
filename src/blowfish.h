#ifndef BLOWFISH_H
#define BLOWFISH_H
#include <stdint.h>

#define BF_IV_LEN 8

typedef struct {
  uint32_t S[4][256];
  uint32_t P[18];
} bf_state_t;

void bf_init(bf_state_t *st, void *key, size_t keylen);
void bf_ecb_decrypt(const bf_state_t *st, void *data, size_t len);
void bf_cbc_encrypt(const bf_state_t *st, void *data, size_t len, void *iv);
void bf_cbc_decrypt(const bf_state_t *st, void *data, size_t len, void *iv);

#endif
