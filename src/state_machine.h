#ifndef SYSU_AUTHD_STATE_MACHINE_H
#define SYSU_AUTHD_STATE_MACHINE_H

#include <stdbool.h>
#include <stdint.h>

#include "auth_backend.h"
#include "config.h"
#include "profile.h"

typedef enum {
	STATE_INIT = 0,
	STATE_LOAD_CONFIG,
	STATE_WAIT_LINK,
	STATE_PREPARE_INTERFACE,
	STATE_AUTHENTICATING,
	STATE_AUTH_SUCCESS,
	STATE_DHCP_RENEWING,
	STATE_ONLINE,
	STATE_AUTH_FAILED,
	STATE_RECOVERING,
	STATE_STOPPED
} auth_state_t;

typedef enum {
	SM_EVENT_NONE = 0,
	SM_EVENT_START,
	SM_EVENT_CONFIG_LOADED,
	SM_EVENT_LINK_UP,
	SM_EVENT_LINK_DOWN,
	SM_EVENT_INTERFACE_READY,
	SM_EVENT_AUTH_SUCCESS,
	SM_EVENT_AUTH_FAILURE,
	SM_EVENT_AUTH_TIMEOUT,
	SM_EVENT_DHCP_SUCCESS,
	SM_EVENT_DHCP_FAILURE,
	SM_EVENT_HEALTH_OK,
	SM_EVENT_HEALTH_FAILED,
	SM_EVENT_RETRY_TIMER,
	SM_EVENT_STOP
} sm_event_t;

typedef struct {
	config_t *config;
	profile_t *profile;
	auth_backend_t *backend;
	const char *password;
	bool dry_run;
} state_machine_deps_t;

typedef struct {
	auth_state_t state;
	auth_state_t previous_state;
	uint64_t entered_at_ms;
	uint64_t auth_started_at_ms;
	uint64_t retry_after_ms;
	uint64_t dhcp_started_at_ms;
	uint64_t last_online_check_ms;
	bool dhcp_renew_requested;
	uint64_t last_success_time;
	unsigned int retry_count;
	char last_error[SYSU_AUTHD_MAX_STR];
	state_machine_deps_t deps;
} state_machine_t;

const char *auth_state_to_string(auth_state_t state);
const char *sm_event_to_string(sm_event_t event);

void state_machine_init(state_machine_t *sm, const state_machine_deps_t *deps);
void state_machine_stop(state_machine_t *sm);
int state_machine_dispatch(state_machine_t *sm, sm_event_t event);
int state_machine_tick(state_machine_t *sm, uint64_t now_ms);
bool state_machine_is_stopped(const state_machine_t *sm);

#endif
