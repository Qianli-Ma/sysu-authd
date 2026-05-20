#include "status.h"

#include "daemon.h"
#include "dhcp.h"
#include "log.h"
#include "netif.h"
#include "utils.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void json_write_string(FILE *fp, const char *value)
{
	const unsigned char *p;

	fputc('"', fp);
	if (value != NULL) {
		for (p = (const unsigned char *)value; *p != '\0'; p++) {
			if (*p == '"' || *p == '\\') {
				fputc('\\', fp);
				fputc((int)*p, fp);
			} else if (*p == '\n') {
				fputs("\\n", fp);
			} else if (*p == '\r') {
				fputs("\\r", fp);
			} else if (*p == '\t') {
				fputs("\\t", fp);
			} else if (*p < 0x20U) {
				fprintf(fp, "\\u%04x", *p);
			} else {
				fputc((int)*p, fp);
			}
		}
	}
	fputc('"', fp);
}

void status_collect(status_snapshot_t *snapshot, const void *daemon_ctx)
{
	const daemon_ctx_t *ctx = daemon_ctx;
	dhcp_status_t dhcp_status;

	if (snapshot == NULL || ctx == NULL)
		return;

	memset(snapshot, 0, sizeof(*snapshot));
	snapshot->enabled = ctx->config.enabled;
	snapshot->state = auth_state_to_string(ctx->sm.state);
	(void)safe_strcpy(snapshot->interface, sizeof(snapshot->interface),
			  ctx->config.interface);
	(void)safe_strcpy(snapshot->device, sizeof(snapshot->device),
			  ctx->config.device);
	(void)safe_strcpy(snapshot->profile, sizeof(snapshot->profile),
			  ctx->profile.profile_name);
	(void)safe_strcpy(snapshot->backend, sizeof(snapshot->backend),
			  ctx->backend.name);
	(void)safe_strcpy(snapshot->last_error, sizeof(snapshot->last_error),
			  ctx->sm.last_error);
	snapshot->last_success_time = ctx->sm.last_success_time;
	snapshot->retry_count = ctx->sm.retry_count;
	snapshot->link_status = ctx->dry_run ||
				netif_is_link_up(ctx->config.device);

	if (dhcp_get_status(ctx->config.interface, &dhcp_status) == 0) {
		(void)safe_strcpy(snapshot->wan_ip, sizeof(snapshot->wan_ip),
				  dhcp_status.ip);
		(void)safe_strcpy(snapshot->gateway, sizeof(snapshot->gateway),
				  dhcp_status.gateway);
		(void)safe_strcpy(snapshot->dns, sizeof(snapshot->dns),
				  dhcp_status.dns);
	}
}

void status_log_debug(const status_snapshot_t *snapshot)
{
	if (snapshot == NULL)
		return;

	LOG_DEBUG("status: state=%s interface=%s device=%s profile=%s backend=%s retry=%u link=%s ip=%s gateway=%s dns=%s",
		  snapshot->state,
		  snapshot->interface,
		  snapshot->device[0] != '\0' ? snapshot->device : "(auto)",
		  snapshot->profile,
		  snapshot->backend,
		  snapshot->retry_count,
		  snapshot->link_status ? "up" : "down",
		  snapshot->wan_ip[0] != '\0' ? snapshot->wan_ip : "(none)",
		  snapshot->gateway[0] != '\0' ? snapshot->gateway : "(none)",
		  snapshot->dns[0] != '\0' ? snapshot->dns : "(none)");
}

int status_write_json(const status_snapshot_t *snapshot, const char *path)
{
	char tmp_path[SYSU_AUTHD_MAX_PATH + 16U];
	FILE *fp;

	if (snapshot == NULL || path == NULL || *path == '\0')
		return -1;
	if (snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path) >=
	    (int)sizeof(tmp_path))
		return -1;

	fp = fopen(tmp_path, "w");
	if (fp == NULL)
		return -1;

	fputs("{\n", fp);
	fprintf(fp, "  \"enabled\": %s,\n", snapshot->enabled ? "true" : "false");
	fputs("  \"state\": ", fp);
	json_write_string(fp, snapshot->state);
	fputs(",\n  \"interface\": ", fp);
	json_write_string(fp, snapshot->interface);
	fputs(",\n  \"device\": ", fp);
	json_write_string(fp, snapshot->device);
	fputs(",\n  \"profile\": ", fp);
	json_write_string(fp, snapshot->profile);
	fputs(",\n  \"backend\": ", fp);
	json_write_string(fp, snapshot->backend);
	fprintf(fp, ",\n  \"last_success_time\": %llu,\n",
		(unsigned long long)snapshot->last_success_time);
	fputs("  \"last_error\": ", fp);
	json_write_string(fp, snapshot->last_error);
	fprintf(fp, ",\n  \"retry_count\": %u,\n", snapshot->retry_count);
	fputs("  \"wan_ip\": ", fp);
	json_write_string(fp, snapshot->wan_ip);
	fputs(",\n  \"gateway\": ", fp);
	json_write_string(fp, snapshot->gateway);
	fputs(",\n  \"dns\": ", fp);
	json_write_string(fp, snapshot->dns);
	fprintf(fp, ",\n  \"link_status\": %s\n",
		snapshot->link_status ? "true" : "false");
	fputs("}\n", fp);

	if (fclose(fp) != 0) {
		(void)unlink(tmp_path);
		return -1;
	}

	if (rename(tmp_path, path) != 0) {
		(void)unlink(tmp_path);
		return -1;
	}

	return 0;
}
