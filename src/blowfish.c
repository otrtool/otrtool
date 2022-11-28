/*
  Blowfish algorithm, as described in [1],
  implemented in C for the otrtool project.

  ** Modified endianess **
  Standard Blowfish is big-endian, left-to-right
  (i.e. left = first = most significant).
  otrtool uses a modified algorithm, in which 32-bit words of the data and IV
  are interpreted as little-endian (everything else stays the same).

  [1]
  https://www.schneier.com/academic/archives/1994/09/description_of_a_new.html
*/

#include <string.h>
#include "blowfish.h"
#include "blowfish_const.h"
#define SWAP(x,y,tmp) do { (tmp) = (x); (x) = (y); (y) = (tmp); } while(0)

static uint32_t F(const bf_state_t *st, uint32_t xL)
{
  uint8_t a, b, c, d;

  a = (xL >> 24) & 0xFF;
  b = (xL >> 16) & 0xFF;
  c = (xL >> 8) & 0xFF;
  d = xL & 0xFF;

  return ((st->S[0][a] + st->S[1][b]) ^ st->S[2][c]) + st->S[3][d];
}

#define BF_ENCRYPT(st, xL, xR) do { \
  uint32_t tmp;                     \
  int i;                            \
                                    \
  for (i=0; i<16; i++) {            \
    xL = xL ^ st->P[i];             \
    xR = F(st, xL) ^ xR;            \
    SWAP(xL, xR, tmp);              \
  }                                 \
  SWAP(xL, xR, tmp);                \
  xR = xR ^ st->P[16];              \
  xL = xL ^ st->P[17];              \
} while(0)

#define BF_DECRYPT(st, xL, xR) do { \
  uint32_t tmp;                     \
  int i;                            \
                                    \
  for (i=17; i>1; i--) {            \
    xL = xL ^ st->P[i];             \
    xR = F(st, xL) ^ xR;            \
    SWAP(xL, xR, tmp);              \
  }                                 \
  SWAP(xL, xR, tmp);                \
  xR = xR ^ st->P[1];               \
  xL = xL ^ st->P[0];               \
} while(0)

void bf_init(bf_state_t *st, void *key_, size_t keylen)
{
  uint8_t *key = key_;
  int i, j;
  size_t ki;
  uint32_t k;
  uint32_t xL, xR;

  if (keylen < 1 || keylen > 56)
    return;

  /* step 1 */
  memcpy(st->S, S0, sizeof(S0));
  memcpy(st->P, P0, sizeof(P0));

  /* step 2 */
  ki = 0;
  for (i=0; i<18; i++) {
    k = 0;
    for (j=0; j<4; j++) {
      k = (k << 8) | key[ki];
      if (++ki >= keylen) ki = 0;
    }
    st->P[i] ^= k;
  }

  /* steps 3 to 7 */
  xL = xR = 0;
  for (i=0; i<18; i+=2) {
    BF_ENCRYPT(st, xL, xR);
    st->P[i] = xL;
    st->P[i+1] = xR;
  }
  for (i=0; i<4; i++) {
    for (j=0; j<256; j+=2) {
      BF_ENCRYPT(st, xL, xR);
      st->S[i][j] = xL;
      st->S[i][j+1] = xR;
    }
  }
}

/* Helper functions for endianness conversion
   They are hopefully optimized out on little-endian platforms. */
static uint16_t load_uint16le(uint8_t *b) {
  return (uint16_t)b[1] << 8 | b[0];
}
static uint32_t load_uint32le(uint8_t *b) {
  return (uint32_t)load_uint16le(b+2) << 16 | load_uint16le(b);
}
static void store_uint16le(uint8_t *b, uint16_t x) {
  b[1] = x >> 8; b[0] = x;
}
static void store_uint32le(uint8_t *b, uint32_t x) {
  store_uint16le(b+2, x >> 16); store_uint16le(b, x);
}

/*
  High-level functions for encryption/decryption in ECB and CBC mode
  No padding of input data is performed.
*/

void bf_ecb_decrypt(const bf_state_t *st, void *data_, size_t len)
{
  uint8_t *data = data_;
  uint32_t xL, xR;
  size_t i;

  for (i=0; i<=len-8; i+=8) {
    xL = load_uint32le(data+i);
    xR = load_uint32le(data+i+4);
    BF_DECRYPT(st, xL, xR);
    store_uint32le(data+i, xL);
    store_uint32le(data+i+4, xR);
  }
}

void bf_cbc_encrypt(const bf_state_t *st, void *data_, size_t len, void *iv_)
{
  uint8_t *data = data_;
  uint8_t *iv = iv_;
  uint32_t xL, xR;
  uint32_t ivL, ivR;
  size_t i;

  ivL = load_uint32le(iv);
  ivR = load_uint32le(iv+4);

  for (i=0; i<=len-8; i+=8) {
    xL = load_uint32le(data+i) ^ ivL;
    xR = load_uint32le(data+i+4) ^ ivR;
    BF_ENCRYPT(st, xL, xR);
    store_uint32le(data+i, xL);
    store_uint32le(data+i+4, xR);
    ivL = xL;
    ivR = xR;
  }

  store_uint32le(iv, ivL);
  store_uint32le(iv+4, ivR);
}

void bf_cbc_decrypt(const bf_state_t *st, void *data_, size_t len, void *iv_)
{
  uint8_t *data = data_;
  uint8_t *iv = iv_;
  uint32_t xL, xR;
  uint32_t ivL, ivR, ivLn, ivRn;
  size_t i;

  ivL = load_uint32le(iv);
  ivR = load_uint32le(iv+4);

  for (i=0; i<=len-8; i+=8) {
    xL = ivLn = load_uint32le(data+i);
    xR = ivRn = load_uint32le(data+i+4);
    BF_DECRYPT(st, xL, xR);
    store_uint32le(data+i, xL ^ ivL);
    store_uint32le(data+i+4, xR ^ ivR);
    ivL = ivLn;
    ivR = ivRn;
  }

  store_uint32le(iv, ivL);
  store_uint32le(iv+4, ivR);
}
