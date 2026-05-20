#ifndef SYSU_AUTHD_DAEMON_H
#define SYSU_AUTHD_DAEMON_H

#include <stdbool.h>
#include <signal.h>

#include "auth_backend.h"
#include "config.h"
#include "profile.h"
#include "state_machine.h"

typedef struct {
	config_t config;
	profile_t profile;
	auth_backend_t backend;
	state_machine_t sm;
	char password[SYSU_AUTHD_PASSWORD_MAX];
	bool dry_run;
	bool foreground;
	volatile sig_atomic_t stop_requested;
} daemon_ctx_t;

int daemon_init(daemon_ctx_t *ctx, const config_t *cfg,
		const profile_t *profile, bool dry_run, bool foreground);
int daemon_run(daemon_ctx_t *ctx);
void daemon_request_stop(daemon_ctx_t *ctx);
void daemon_cleanup(daemon_ctx_t *ctx);

#endif
