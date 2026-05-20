#define _DEFAULT_SOURCE

#include "log.h"

#include <stdio.h>
#include <string.h>
#include <syslog.h>

static log_level_t g_log_level = LOG_LEVEL_INFO;
static bool g_foreground = true;
static bool g_syslog_opened;

static int to_syslog_priority(log_level_t level)
{
	switch (level) {
	case LOG_LEVEL_ERROR:
		return LOG_ERR;
	case LOG_LEVEL_WARN:
		return LOG_WARNING;
	case LOG_LEVEL_INFO:
		return LOG_INFO;
	case LOG_LEVEL_DEBUG:
		return LOG_DEBUG;
	default:
		return LOG_INFO;
	}
}

void log_init(log_level_t level, bool foreground)
{
	g_log_level = level;
	g_foreground = foreground;

	if (!foreground && !g_syslog_opened) {
		openlog("sysu-authd", LOG_PID | LOG_NDELAY, LOG_DAEMON);
		g_syslog_opened = true;
	}
}

void log_set_level(log_level_t level)
{
	g_log_level = level;
}

log_level_t log_level_from_string(const char *value, log_level_t fallback)
{
	if (value == NULL || *value == '\0')
		return fallback;
	if (strcmp(value, "error") == 0)
		return LOG_LEVEL_ERROR;
	if (strcmp(value, "warn") == 0 || strcmp(value, "warning") == 0)
		return LOG_LEVEL_WARN;
	if (strcmp(value, "info") == 0)
		return LOG_LEVEL_INFO;
	if (strcmp(value, "debug") == 0)
		return LOG_LEVEL_DEBUG;
	return fallback;
}

const char *log_level_to_string(log_level_t level)
{
	switch (level) {
	case LOG_LEVEL_ERROR:
		return "error";
	case LOG_LEVEL_WARN:
		return "warn";
	case LOG_LEVEL_INFO:
		return "info";
	case LOG_LEVEL_DEBUG:
		return "debug";
	default:
		return "unknown";
	}
}

void log_vmessage(log_level_t level, const char *fmt, va_list ap)
{
	va_list copy;

	if (level > g_log_level)
		return;

	if (g_foreground) {
		fprintf(stderr, "[%s] ", log_level_to_string(level));
		vfprintf(stderr, fmt, ap);
		fputc('\n', stderr);
		return;
	}

	va_copy(copy, ap);
	vsyslog(to_syslog_priority(level), fmt, copy);
	va_end(copy);
}

void log_message(log_level_t level, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	log_vmessage(level, fmt, ap);
	va_end(ap);
}
