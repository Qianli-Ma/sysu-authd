#ifndef SYSU_AUTHD_MD5_H
#define SYSU_AUTHD_MD5_H

#include <stddef.h>
#include <stdint.h>

#define MD5_DIGEST_LEN 16U

typedef struct {
	uint32_t state[4];
	uint64_t bit_count;
	uint8_t buffer[64];
} md5_ctx_t;

void md5_init(md5_ctx_t *ctx);
void md5_update(md5_ctx_t *ctx, const uint8_t *data, size_t len);
void md5_final(md5_ctx_t *ctx, uint8_t digest[MD5_DIGEST_LEN]);
void md5_digest(const uint8_t *data, size_t len, uint8_t digest[MD5_DIGEST_LEN]);

#endif
