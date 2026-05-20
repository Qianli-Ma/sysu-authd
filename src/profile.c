#include "profile.h"

#include "utils.h"

#include <string.h>

void profile_set_defaults(profile_t *profile)
{
	memset(profile, 0, sizeof(*profile));

	(void)safe_strcpy(profile->profile_name, sizeof(profile->profile_name), "standard");
	profile->eapol_version = 1;
	(void)safe_strcpy(profile->identity_format, sizeof(profile->identity_format), "%u");
	(void)safe_strcpy(profile->preferred_backend, sizeof(profile->preferred_backend),
			  "standard_eapol");
	profile->need_keepalive = false;
	profile->dhcp_after_success = true;
	(void)safe_strcpy(profile->timeout_policy, sizeof(profile->timeout_policy), "default");
	profile->retry_policy.initial_delay_sec = 3;
	profile->retry_policy.max_delay_sec = 300;
	profile->retry_policy.forever = true;
	(void)safe_strcpy(profile->healthcheck_target, sizeof(profile->healthcheck_target),
			  "223.5.5.5");
	profile->compat_flags = PROFILE_COMPAT_NONE;
}

int profile_load(profile_t *profile, const char *name)
{
	profile_set_defaults(profile);

	if (name == NULL || *name == '\0' || strcmp(name, "standard") == 0)
		return 0;

	if (strcmp(name, "sysu_ruijie") == 0) {
		(void)safe_strcpy(profile->profile_name, sizeof(profile->profile_name),
				  "sysu_ruijie");
		(void)safe_strcpy(profile->preferred_backend,
				  sizeof(profile->preferred_backend), "ruijie_compat");
		profile->need_keepalive = true;
		profile->dhcp_after_success = true;
		profile->compat_flags = PROFILE_COMPAT_RUIJIE |
					PROFILE_COMPAT_KEEPALIVE_AUTO;
		return 0;
	}

	return -1;
}

void profile_apply_to_config(const profile_t *profile, config_t *cfg)
{
	if (profile == NULL || cfg == NULL)
		return;

	if (cfg->auth_backend[0] == '\0')
		(void)safe_strcpy(cfg->auth_backend, sizeof(cfg->auth_backend),
				  profile->preferred_backend);

	if (cfg->identity_format[0] == '\0')
		(void)safe_strcpy(cfg->identity_format, sizeof(cfg->identity_format),
				  profile->identity_format);

	if (cfg->eapol_version == 0)
		cfg->eapol_version = profile->eapol_version;
}
