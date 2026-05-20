#ifndef SYSU_AUTHD_STATUS_H
#define SYSU_AUTHD_STATUS_H

#include <stdbool.h>
#include <stdint.h>

#include "config.h"
#include "state_machine.h"

#define SYSU_AUTHD_STATUS_FILE "/var/run/sysu-authd.status"

struct daemon_ctx;
typedef struct daemon_ctx daemon_ctx_alias_t;

typedef struct {
	bool enabled;
	const char *state;
	char interface[SYSU_AUTHD_MAX_STR];
	char device[SYSU_AUTHD_MAX_STR];
	char profile[SYSU_AUTHD_MAX_STR];
	char backend[SYSU_AUTHD_MAX_STR];
	uint64_t last_success_time;
	char last_error[SYSU_AUTHD_MAX_STR];
	unsigned int retry_count;
	char wan_ip[SYSU_AUTHD_MAX_STR];
	char gateway[SYSU_AUTHD_MAX_STR];
	char dns[SYSU_AUTHD_MAX_STR];
	bool link_status;
} status_snapshot_t;

void status_collect(status_snapshot_t *snapshot, const void *daemon_ctx);
void status_log_debug(const status_snapshot_t *snapshot);
int status_write_json(const status_snapshot_t *snapshot, const char *path);

#endif
