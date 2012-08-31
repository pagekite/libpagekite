/* public api for steve reid's public domain SHA-1 implementation */
/* this file is in the public domain */

#ifndef __SHA1_H
#define __SHA1_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    uint8_t  buffer[64];
} SHA1_CTX;

#define SHA1_DIGEST_SIZE 20

void sha1_init(SHA1_CTX* context);
void sha1_update(SHA1_CTX* context, const uint8_t* data, const size_t len);
void sha1_final(SHA1_CTX* context, uint8_t digest[SHA1_DIGEST_SIZE]);
void digest_to_hex(const uint8_t digest[SHA1_DIGEST_SIZE], char *output);

#ifdef __cplusplus
}
#endif

#endif /* __SHA1_H */
