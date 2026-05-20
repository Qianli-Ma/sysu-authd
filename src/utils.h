#ifndef SYSU_AUTHD_UTILS_H
#define SYSU_AUTHD_UTILS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

typedef struct {
	uint64_t ms;
} monotonic_time_t;

uint64_t now_msec(void);
void sleep_msec(unsigned int msec);
char *trim_ascii(char *s);
bool parse_bool(const char *value, bool fallback);
int parse_int(const char *value, int fallback);
int safe_strcpy(char *dst, size_t dst_len, const char *src);
const char *nonnull(const char *value);

#endif
