/* public api for steve reid's public domain SHA-1 implementation */
/* this file is in the public domain */

#ifndef __PD_SHA1_H
#define __PD_SHA1_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    uint8_t  buffer[64];
} PD_SHA1_CTX;

#define SHA1_DIGEST_SIZE 20

void pd_sha1_init(PD_SHA1_CTX* context);
void pd_sha1_update(PD_SHA1_CTX* context, const uint8_t* data, const size_t len);
void pd_sha1_final(PD_SHA1_CTX* context, uint8_t digest[SHA1_DIGEST_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* __PD_SHA1_H */
