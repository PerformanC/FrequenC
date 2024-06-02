/*
  licenses/sha1.license

  * Contains prefix edits to avoid conflicts with OpenSSL.
  * Fixed macros to remove additional ;
*/

#ifndef SHA1C_H
#define SHA1C_H

#include "stdint.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct
{
    uint32_t state[5];
    uint32_t count[2];
    unsigned char buffer[64];
} SHA1C_CTX;

void SHA1CTransform(
    uint32_t state[5],
    const unsigned char buffer[64]
    );

void SHA1CInit(
    SHA1C_CTX * context
    );

void SHA1CUpdate(
    SHA1C_CTX * context,
    const unsigned char *data,
    uint32_t len
    );

void SHA1CFinal(
    unsigned char digest[20],
    SHA1C_CTX * context
    );

void SHA1C(
    char *hash_out,
    const char *str,
    uint32_t len);

#if defined(__cplusplus)
}
#endif

#endif /* SHA1C_H */
