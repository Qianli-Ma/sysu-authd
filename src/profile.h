#ifndef SYSU_AUTHD_PROFILE_H
#define SYSU_AUTHD_PROFILE_H

#include <stdbool.h>
#include <stdint.h>

#include "config.h"

typedef enum {
	PROFILE_COMPAT_NONE = 0,
	PROFILE_COMPAT_RUIJIE = 1U << 0,
	PROFILE_COMPAT_KEEPALIVE_AUTO = 1U << 1
} profile_compat_flags_t;

typedef struct {
	char profile_name[SYSU_AUTHD_MAX_STR];
	uint8_t eapol_version;
	char identity_format[SYSU_AUTHD_MAX_STR];
	char preferred_backend[SYSU_AUTHD_MAX_STR];
	bool need_keepalive;
	bool dhcp_after_success;
	char timeout_policy[SYSU_AUTHD_MAX_STR];
	retry_policy_t retry_policy;
	char healthcheck_target[SYSU_AUTHD_MAX_STR];
	uint32_t compat_flags;
} profile_t;

void profile_set_defaults(profile_t *profile);
int profile_load(profile_t *profile, const char *name);
void profile_apply_to_config(const profile_t *profile, config_t *cfg);

#endif
