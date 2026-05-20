#define _POSIX_C_SOURCE 200809L

#include "utils.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

uint64_t now_msec(void)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
		return 0;

	return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000ULL);
}

void sleep_msec(unsigned int msec)
{
	struct timespec req;
	struct timespec rem;

	req.tv_sec = (time_t)(msec / 1000U);
	req.tv_nsec = (long)(msec % 1000U) * 1000000L;

	while (nanosleep(&req, &rem) != 0 && errno == EINTR)
		req = rem;
}

char *trim_ascii(char *s)
{
	char *end;

	if (s == NULL)
		return NULL;

	while (isspace((unsigned char)*s))
		s++;

	if (*s == '\0')
		return s;

	end = s + strlen(s) - 1;
	while (end > s && isspace((unsigned char)*end)) {
		*end = '\0';
		end--;
	}

	return s;
}

bool parse_bool(const char *value, bool fallback)
{
	if (value == NULL)
		return fallback;
	if (strcmp(value, "1") == 0 || strcmp(value, "true") == 0 ||
	    strcmp(value, "yes") == 0 || strcmp(value, "on") == 0 ||
	    strcmp(value, "enabled") == 0)
		return true;
	if (strcmp(value, "0") == 0 || strcmp(value, "false") == 0 ||
	    strcmp(value, "no") == 0 || strcmp(value, "off") == 0 ||
	    strcmp(value, "disabled") == 0)
		return false;
	return fallback;
}

int parse_int(const char *value, int fallback)
{
	char *end = NULL;
	long parsed;

	if (value == NULL || *value == '\0')
		return fallback;

	errno = 0;
	parsed = strtol(value, &end, 10);
	if (errno != 0 || end == value || *end != '\0' ||
	    parsed < INT_MIN || parsed > INT_MAX)
		return fallback;

	return (int)parsed;
}

int safe_strcpy(char *dst, size_t dst_len, const char *src)
{
	size_t src_len;

	if (dst == NULL || dst_len == 0)
		return -1;

	if (src == NULL)
		src = "";

	src_len = strlen(src);
	if (src_len >= dst_len) {
		dst[0] = '\0';
		return -1;
	}

	memcpy(dst, src, src_len + 1);
	return 0;
}

const char *nonnull(const char *value)
{
	return value != NULL ? value : "";
}
