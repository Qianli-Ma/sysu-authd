#ifndef SYSU_AUTHD_LOG_H
#define SYSU_AUTHD_LOG_H

#include <stdarg.h>
#include <stdbool.h>

typedef enum {
	LOG_LEVEL_ERROR = 0,
	LOG_LEVEL_WARN,
	LOG_LEVEL_INFO,
	LOG_LEVEL_DEBUG
} log_level_t;

void log_init(log_level_t level, bool foreground);
void log_set_level(log_level_t level);
log_level_t log_level_from_string(const char *value, log_level_t fallback);
const char *log_level_to_string(log_level_t level);

void log_message(log_level_t level, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));
void log_vmessage(log_level_t level, const char *fmt, va_list ap);

#define LOG_ERROR(...) log_message(LOG_LEVEL_ERROR, __VA_ARGS__)
#define LOG_WARN(...)  log_message(LOG_LEVEL_WARN, __VA_ARGS__)
#define LOG_INFO(...)  log_message(LOG_LEVEL_INFO, __VA_ARGS__)
#define LOG_DEBUG(...) log_message(LOG_LEVEL_DEBUG, __VA_ARGS__)

#endif
