#pragma once
#ifdef __cplusplus
extern "C" {
#endif
int crypto_sign(
  unsigned char *sm,unsigned long long *smlen,
  const unsigned char *m,unsigned long long mlen,
  const unsigned char *sk
);

int crypto_sign_open(
  unsigned char* m,unsigned long long* mlen,
  const unsigned char* sm,unsigned long long smlen,
  const unsigned char* pk
);

int crypto_sign_keypair(
  unsigned char* pk,unsigned char* sk
);
#ifdef __cplusplus
}
#endif
