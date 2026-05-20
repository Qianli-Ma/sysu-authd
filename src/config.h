#ifndef SYSU_AUTHD_CONFIG_H
#define SYSU_AUTHD_CONFIG_H

#include "log.h"

#include <stdbool.h>
#include <stddef.h>

#define SYSU_AUTHD_MAX_STR 128
#define SYSU_AUTHD_MAX_PATH 256
#define SYSU_AUTHD_PASSWORD_MAX 256

typedef struct {
	int initial_delay_sec;
	int max_delay_sec;
	bool forever;
} retry_policy_t;

typedef struct {
	bool enabled;
	char interface[SYSU_AUTHD_MAX_STR];
	char device[SYSU_AUTHD_MAX_STR];
	char username[SYSU_AUTHD_MAX_STR];
	char password_file[SYSU_AUTHD_MAX_PATH];
	char profile[SYSU_AUTHD_MAX_STR];
	char auth_backend[SYSU_AUTHD_MAX_STR];
	char identity_format[SYSU_AUTHD_MAX_STR];
	unsigned int eapol_version;
	bool dhcp_after_success;
	bool healthcheck_enable;
	bool logoff_on_stop;
	bool reauth_enable;
	retry_policy_t retry_policy;
	log_level_t log_level;
	unsigned int auth_timeout_sec;
	unsigned int dhcp_timeout_sec;
	unsigned int online_check_interval_sec;
	unsigned int tick_interval_msec;
} config_t;

void config_set_defaults(config_t *cfg);
int config_load_file(config_t *cfg, const char *path);
int config_read_password(const config_t *cfg, char *buf, size_t buf_len);
void config_sanitize_for_log(config_t *cfg);

#endif
