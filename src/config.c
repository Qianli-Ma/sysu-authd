#define _POSIX_C_SOURCE 200809L

#include "config.h"

#include "log.h"
#include "utils.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static int parse_retry_policy(retry_policy_t *policy, const char *value)
{
	char tmp[SYSU_AUTHD_MAX_STR];
	char *part;
	char *saveptr = NULL;

	if (value == NULL || safe_strcpy(tmp, sizeof(tmp), value) != 0)
		return -1;

	part = strtok_r(tmp, ",", &saveptr);
	while (part != NULL) {
		char *eq;
		char *key;
		char *val;

		eq = strchr(part, ':');
		if (eq == NULL)
			eq = strchr(part, '=');
		if (eq == NULL)
			return -1;

		*eq = '\0';
		key = trim_ascii(part);
		val = trim_ascii(eq + 1);

		if (strcmp(key, "initial") == 0)
			policy->initial_delay_sec = parse_int(val, policy->initial_delay_sec);
		else if (strcmp(key, "max") == 0)
			policy->max_delay_sec = parse_int(val, policy->max_delay_sec);
		else if (strcmp(key, "forever") == 0)
			policy->forever = parse_bool(val, policy->forever);
		else
			return -1;

		part = strtok_r(NULL, ",", &saveptr);
	}

	if (policy->initial_delay_sec < 1)
		policy->initial_delay_sec = 1;
	if (policy->max_delay_sec < policy->initial_delay_sec)
		policy->max_delay_sec = policy->initial_delay_sec;

	return 0;
}

static int set_config_value(config_t *cfg, const char *key, const char *value)
{
	if (strcmp(key, "enabled") == 0) {
		cfg->enabled = parse_bool(value, cfg->enabled);
	} else if (strcmp(key, "interface") == 0) {
		return safe_strcpy(cfg->interface, sizeof(cfg->interface), value);
	} else if (strcmp(key, "device") == 0) {
		return safe_strcpy(cfg->device, sizeof(cfg->device), value);
	} else if (strcmp(key, "username") == 0) {
		return safe_strcpy(cfg->username, sizeof(cfg->username), value);
	} else if (strcmp(key, "password_file") == 0) {
		return safe_strcpy(cfg->password_file, sizeof(cfg->password_file), value);
	} else if (strcmp(key, "profile") == 0) {
		return safe_strcpy(cfg->profile, sizeof(cfg->profile), value);
	} else if (strcmp(key, "auth_backend") == 0) {
		return safe_strcpy(cfg->auth_backend, sizeof(cfg->auth_backend), value);
	} else if (strcmp(key, "identity_format") == 0) {
		return safe_strcpy(cfg->identity_format, sizeof(cfg->identity_format), value);
	} else if (strcmp(key, "eapol_version") == 0) {
		cfg->eapol_version = (unsigned int)parse_int(value, (int)cfg->eapol_version);
	} else if (strcmp(key, "dhcp_after_success") == 0) {
		cfg->dhcp_after_success = parse_bool(value, cfg->dhcp_after_success);
	} else if (strcmp(key, "healthcheck_enable") == 0) {
		cfg->healthcheck_enable = parse_bool(value, cfg->healthcheck_enable);
	} else if (strcmp(key, "logoff_on_stop") == 0) {
		cfg->logoff_on_stop = parse_bool(value, cfg->logoff_on_stop);
	} else if (strcmp(key, "reauth_enable") == 0) {
		cfg->reauth_enable = parse_bool(value, cfg->reauth_enable);
	} else if (strcmp(key, "retry_policy") == 0) {
		return parse_retry_policy(&cfg->retry_policy, value);
	} else if (strcmp(key, "log_level") == 0) {
		cfg->log_level = log_level_from_string(value, cfg->log_level);
	} else if (strcmp(key, "auth_timeout_sec") == 0) {
		cfg->auth_timeout_sec = (unsigned int)parse_int(value, (int)cfg->auth_timeout_sec);
	} else if (strcmp(key, "dhcp_timeout_sec") == 0) {
		cfg->dhcp_timeout_sec = (unsigned int)parse_int(value, (int)cfg->dhcp_timeout_sec);
	} else if (strcmp(key, "online_check_interval_sec") == 0) {
		cfg->online_check_interval_sec =
			(unsigned int)parse_int(value, (int)cfg->online_check_interval_sec);
	} else if (strcmp(key, "tick_interval_msec") == 0) {
		cfg->tick_interval_msec = (unsigned int)parse_int(value, (int)cfg->tick_interval_msec);
	} else {
		LOG_WARN("unknown config key ignored: %s", key);
	}

	return 0;
}

void config_set_defaults(config_t *cfg)
{
	memset(cfg, 0, sizeof(*cfg));

	cfg->enabled = true;
	(void)safe_strcpy(cfg->interface, sizeof(cfg->interface), "wan");
	(void)safe_strcpy(cfg->device, sizeof(cfg->device), "");
	(void)safe_strcpy(cfg->username, sizeof(cfg->username), "");
	(void)safe_strcpy(cfg->password_file, sizeof(cfg->password_file),
			  "/etc/sysu-authd/password");
	(void)safe_strcpy(cfg->profile, sizeof(cfg->profile), "sysu_ruijie");
	(void)safe_strcpy(cfg->auth_backend, sizeof(cfg->auth_backend), "ruijie_compat");
	(void)safe_strcpy(cfg->identity_format, sizeof(cfg->identity_format), "%u");
	cfg->eapol_version = 1;
	cfg->dhcp_after_success = true;
	cfg->healthcheck_enable = true;
	cfg->logoff_on_stop = true;
	cfg->reauth_enable = true;
	cfg->retry_policy.initial_delay_sec = 3;
	cfg->retry_policy.max_delay_sec = 300;
	cfg->retry_policy.forever = true;
	cfg->log_level = LOG_LEVEL_INFO;
	cfg->auth_timeout_sec = 30;
	cfg->dhcp_timeout_sec = 30;
	cfg->online_check_interval_sec = 10;
	cfg->tick_interval_msec = 250;
}

int config_load_file(config_t *cfg, const char *path)
{
	FILE *fp;
	char line[512];
	unsigned int line_no = 0;

	if (path == NULL || *path == '\0')
		return 0;

	fp = fopen(path, "r");
	if (fp == NULL) {
		LOG_ERROR("failed to open config %s: %s", path, strerror(errno));
		return -1;
	}

	while (fgets(line, sizeof(line), fp) != NULL) {
		char *p;
		char *eq;
		char *key;
		char *value;
		char *comment;

		line_no++;
		p = trim_ascii(line);
		if (*p == '\0' || *p == '#')
			continue;

		comment = strchr(p, '#');
		if (comment != NULL) {
			*comment = '\0';
			p = trim_ascii(p);
		}

		eq = strchr(p, '=');
		if (eq == NULL) {
			LOG_ERROR("invalid config line %u in %s", line_no, path);
			fclose(fp);
			return -1;
		}

		*eq = '\0';
		key = trim_ascii(p);
		value = trim_ascii(eq + 1);

		if ((*value == '"' || *value == '\'') && value[strlen(value) - 1] == *value) {
			value[strlen(value) - 1] = '\0';
			value++;
		}

		if (set_config_value(cfg, key, value) != 0) {
			LOG_ERROR("invalid value for config key %s at %s:%u", key, path, line_no);
			fclose(fp);
			return -1;
		}
	}

	fclose(fp);
	return 0;
}

int config_read_password(const config_t *cfg, char *buf, size_t buf_len)
{
	FILE *fp;
	struct stat st;
	char *trimmed;

	if (buf == NULL || buf_len == 0 || cfg == NULL)
		return -1;
	buf[0] = '\0';

	if (cfg->password_file[0] == '\0') {
		LOG_ERROR("password_file is not configured");
		return -1;
	}

	if (stat(cfg->password_file, &st) != 0) {
		LOG_ERROR("failed to stat password file %s: %s",
			  cfg->password_file, strerror(errno));
		return -1;
	}

	if ((st.st_mode & 0077) != 0)
		LOG_WARN("password file %s is readable by group/others; recommended mode is 0600",
			 cfg->password_file);

	fp = fopen(cfg->password_file, "r");
	if (fp == NULL) {
		LOG_ERROR("failed to open password file %s: %s",
			  cfg->password_file, strerror(errno));
		return -1;
	}

	if (fgets(buf, (int)buf_len, fp) == NULL) {
		fclose(fp);
		LOG_ERROR("password file %s is empty", cfg->password_file);
		return -1;
	}

	fclose(fp);

	trimmed = trim_ascii(buf);
	if (trimmed != buf)
		memmove(buf, trimmed, strlen(trimmed) + 1);

	if (buf[0] == '\0') {
		LOG_ERROR("password file %s contains an empty password", cfg->password_file);
		return -1;
	}

	return 0;
}

void config_sanitize_for_log(config_t *cfg)
{
	if (cfg == NULL)
		return;
}
