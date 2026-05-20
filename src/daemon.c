#include "daemon.h"

#include "log.h"
#include "netif.h"
#include "status.h"
#include "utils.h"

#include <string.h>

int daemon_init(daemon_ctx_t *ctx, const config_t *cfg,
		const profile_t *profile, bool dry_run, bool foreground)
{
	state_machine_deps_t deps;

	if (ctx == NULL || cfg == NULL || profile == NULL)
		return -1;

	memset(ctx, 0, sizeof(*ctx));
	ctx->config = *cfg;
	ctx->profile = *profile;
	ctx->dry_run = dry_run;
	ctx->foreground = foreground;

	if (ctx->config.device[0] == '\0' &&
	    netif_resolve_device(ctx->config.interface, ctx->config.device,
				 sizeof(ctx->config.device)) != 0) {
		LOG_ERROR("failed to resolve device for interface %s",
			  ctx->config.interface);
		return -1;
	}

	if (!ctx->config.enabled) {
		LOG_WARN("sysu-authd is disabled by config");
		return 0;
	}

	if (dry_run) {
		(void)safe_strcpy(ctx->password, sizeof(ctx->password), "dry-run-password");
	} else if (config_read_password(&ctx->config, ctx->password,
					sizeof(ctx->password)) != 0) {
		return -1;
	}

	if (auth_backend_create(ctx->config.auth_backend, &ctx->backend) != 0) {
		LOG_ERROR("unknown auth backend: %s", ctx->config.auth_backend);
		return -1;
	}

	memset(&deps, 0, sizeof(deps));
	deps.config = &ctx->config;
	deps.profile = &ctx->profile;
	deps.backend = &ctx->backend;
	deps.password = ctx->password;
	deps.dry_run = dry_run;

	state_machine_init(&ctx->sm, &deps);

	return 0;
}

int daemon_run(daemon_ctx_t *ctx)
{
	uint64_t last_status_ms = 0;

	if (ctx == NULL)
		return -1;

	if (!ctx->config.enabled)
		return 0;

	LOG_INFO("sysu-authd starting: interface=%s device=%s profile=%s backend=%s",
		 ctx->config.interface,
		 ctx->config.device[0] != '\0' ? ctx->config.device : "(auto)",
		 ctx->profile.profile_name,
		 ctx->backend.name);

	if (state_machine_dispatch(&ctx->sm, SM_EVENT_START) != 0)
		return -1;

	while (!ctx->stop_requested && !state_machine_is_stopped(&ctx->sm)) {
		uint64_t now = now_msec();

		if (state_machine_tick(&ctx->sm, now) != 0)
			return -1;

		if (now - last_status_ms >= 5000U) {
			status_snapshot_t snapshot;

			status_collect(&snapshot, ctx);
			status_log_debug(&snapshot);
			if (status_write_json(&snapshot, SYSU_AUTHD_STATUS_FILE) != 0)
				LOG_DEBUG("failed to write status file: %s",
					  SYSU_AUTHD_STATUS_FILE);
			last_status_ms = now;
		}

		sleep_msec(ctx->config.tick_interval_msec);

		if (ctx->dry_run && ctx->sm.state == STATE_ONLINE)
			break;
	}

	state_machine_stop(&ctx->sm);
	{
		status_snapshot_t snapshot;

		status_collect(&snapshot, ctx);
		(void)status_write_json(&snapshot, SYSU_AUTHD_STATUS_FILE);
	}
	LOG_INFO("sysu-authd stopped");
	return 0;
}

void daemon_request_stop(daemon_ctx_t *ctx)
{
	if (ctx != NULL)
		ctx->stop_requested = 1;
}

void daemon_cleanup(daemon_ctx_t *ctx)
{
	if (ctx == NULL)
		return;

	if (ctx->backend.stop != NULL)
		ctx->backend.stop(&ctx->backend);
	auth_backend_destroy(&ctx->backend);
	memset(ctx->password, 0, sizeof(ctx->password));
}
