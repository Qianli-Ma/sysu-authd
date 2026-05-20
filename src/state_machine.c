#include "state_machine.h"

#include "dhcp.h"
#include "log.h"
#include "netif.h"
#include "utils.h"
#include "watchdog.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static void transition_to(state_machine_t *sm, auth_state_t next)
{
	if (sm->state == next)
		return;

	LOG_INFO("state: %s -> %s",
		 auth_state_to_string(sm->state), auth_state_to_string(next));
	sm->previous_state = sm->state;
	sm->state = next;
	sm->entered_at_ms = now_msec();

	if (next == STATE_AUTHENTICATING)
		sm->auth_started_at_ms = sm->entered_at_ms;
	if (next == STATE_DHCP_RENEWING) {
		sm->dhcp_renew_requested = false;
		sm->dhcp_started_at_ms = sm->entered_at_ms;
	}
	if (next == STATE_ONLINE)
		sm->last_online_check_ms = 0;
}

static void set_error(state_machine_t *sm, const char *message)
{
	if (message == NULL)
		message = "";
	(void)snprintf(sm->last_error, sizeof(sm->last_error), "%s", message);
}

static int begin_authentication(state_machine_t *sm)
{
	auth_backend_init_args_t args;

	if (sm->deps.backend->init == NULL || sm->deps.backend->start == NULL)
		return -1;

	memset(&args, 0, sizeof(args));
	args.config = sm->deps.config;
	args.profile = sm->deps.profile;
	args.password = sm->deps.password;
	args.dry_run = sm->deps.dry_run;

	if (sm->deps.backend->ctx != NULL && sm->deps.backend->destroy != NULL)
		sm->deps.backend->destroy(sm->deps.backend);

	if (sm->deps.backend->init(sm->deps.backend, &args) != 0) {
		set_error(sm, "backend initialization failed");
		return -1;
	}

	if (sm->deps.backend->start(sm->deps.backend) != 0) {
		set_error(sm, "backend start failed");
		return -1;
	}

	transition_to(sm, STATE_AUTHENTICATING);
	return 0;
}

static void schedule_retry(state_machine_t *sm, uint64_t now_ms)
{
	const retry_policy_t *policy = &sm->deps.config->retry_policy;
	unsigned int delay = (unsigned int)policy->initial_delay_sec;

	if (sm->retry_count > 0) {
		unsigned int i;

		for (i = 0; i < sm->retry_count; i++) {
			if (delay >= (unsigned int)policy->max_delay_sec / 2U) {
				delay = (unsigned int)policy->max_delay_sec;
				break;
			}
			delay *= 2U;
		}
	}

	if (delay > (unsigned int)policy->max_delay_sec)
		delay = (unsigned int)policy->max_delay_sec;

	sm->retry_after_ms = now_ms + (uint64_t)delay * 1000ULL;
	LOG_WARN("retry scheduled in %u second(s)", delay);
}

const char *auth_state_to_string(auth_state_t state)
{
	switch (state) {
	case STATE_INIT:
		return "INIT";
	case STATE_LOAD_CONFIG:
		return "LOAD_CONFIG";
	case STATE_WAIT_LINK:
		return "WAIT_LINK";
	case STATE_PREPARE_INTERFACE:
		return "PREPARE_INTERFACE";
	case STATE_AUTHENTICATING:
		return "AUTHENTICATING";
	case STATE_AUTH_SUCCESS:
		return "AUTH_SUCCESS";
	case STATE_DHCP_RENEWING:
		return "DHCP_RENEWING";
	case STATE_ONLINE:
		return "ONLINE";
	case STATE_AUTH_FAILED:
		return "AUTH_FAILED";
	case STATE_RECOVERING:
		return "RECOVERING";
	case STATE_STOPPED:
		return "STOPPED";
	default:
		return "UNKNOWN";
	}
}

const char *sm_event_to_string(sm_event_t event)
{
	switch (event) {
	case SM_EVENT_NONE:
		return "NONE";
	case SM_EVENT_START:
		return "START";
	case SM_EVENT_CONFIG_LOADED:
		return "CONFIG_LOADED";
	case SM_EVENT_LINK_UP:
		return "LINK_UP";
	case SM_EVENT_LINK_DOWN:
		return "LINK_DOWN";
	case SM_EVENT_INTERFACE_READY:
		return "INTERFACE_READY";
	case SM_EVENT_AUTH_SUCCESS:
		return "AUTH_SUCCESS";
	case SM_EVENT_AUTH_FAILURE:
		return "AUTH_FAILURE";
	case SM_EVENT_AUTH_TIMEOUT:
		return "AUTH_TIMEOUT";
	case SM_EVENT_DHCP_SUCCESS:
		return "DHCP_SUCCESS";
	case SM_EVENT_DHCP_FAILURE:
		return "DHCP_FAILURE";
	case SM_EVENT_HEALTH_OK:
		return "HEALTH_OK";
	case SM_EVENT_HEALTH_FAILED:
		return "HEALTH_FAILED";
	case SM_EVENT_RETRY_TIMER:
		return "RETRY_TIMER";
	case SM_EVENT_STOP:
		return "STOP";
	default:
		return "UNKNOWN";
	}
}

void state_machine_init(state_machine_t *sm, const state_machine_deps_t *deps)
{
	memset(sm, 0, sizeof(*sm));
	sm->state = STATE_INIT;
	sm->previous_state = STATE_INIT;
	sm->entered_at_ms = now_msec();
	sm->deps = *deps;
}

void state_machine_stop(state_machine_t *sm)
{
	(void)state_machine_dispatch(sm, SM_EVENT_STOP);
}

int state_machine_dispatch(state_machine_t *sm, sm_event_t event)
{
	uint64_t now_ms = now_msec();

	LOG_DEBUG("event %s in state %s",
		  sm_event_to_string(event), auth_state_to_string(sm->state));

	if (event == SM_EVENT_STOP) {
		if (sm->deps.backend != NULL && sm->deps.backend->stop != NULL)
			sm->deps.backend->stop(sm->deps.backend);
		transition_to(sm, STATE_STOPPED);
		return 0;
	}

	switch (sm->state) {
	case STATE_INIT:
		if (event == SM_EVENT_START)
			transition_to(sm, STATE_LOAD_CONFIG);
		break;
	case STATE_LOAD_CONFIG:
		if (event == SM_EVENT_CONFIG_LOADED)
			transition_to(sm, STATE_WAIT_LINK);
		break;
	case STATE_WAIT_LINK:
		if (event == SM_EVENT_LINK_UP)
			transition_to(sm, STATE_PREPARE_INTERFACE);
		break;
	case STATE_PREPARE_INTERFACE:
		if (event == SM_EVENT_INTERFACE_READY) {
			if (begin_authentication(sm) != 0) {
				transition_to(sm, STATE_AUTH_FAILED);
				schedule_retry(sm, now_ms);
			}
		}
		break;
	case STATE_AUTHENTICATING:
		if (event == SM_EVENT_AUTH_SUCCESS) {
			sm->retry_count = 0;
			sm->last_success_time = (uint64_t)time(NULL);
			set_error(sm, "");
			transition_to(sm, STATE_AUTH_SUCCESS);
		} else if (event == SM_EVENT_AUTH_FAILURE ||
			   event == SM_EVENT_AUTH_TIMEOUT ||
			   event == SM_EVENT_LINK_DOWN) {
			set_error(sm, sm_event_to_string(event));
			transition_to(sm, STATE_AUTH_FAILED);
			sm->retry_count++;
			schedule_retry(sm, now_ms);
		}
		break;
	case STATE_AUTH_SUCCESS:
		if (event == SM_EVENT_DHCP_SUCCESS)
			transition_to(sm, STATE_ONLINE);
		else if (event == SM_EVENT_DHCP_FAILURE) {
			set_error(sm, "DHCP renewal failed");
			transition_to(sm, STATE_RECOVERING);
			sm->retry_count++;
			schedule_retry(sm, now_ms);
		}
		break;
	case STATE_DHCP_RENEWING:
		if (event == SM_EVENT_DHCP_SUCCESS)
			transition_to(sm, STATE_ONLINE);
		else if (event == SM_EVENT_DHCP_FAILURE) {
			set_error(sm, "DHCP renewal failed");
			transition_to(sm, STATE_RECOVERING);
			sm->retry_count++;
			schedule_retry(sm, now_ms);
		}
		break;
	case STATE_ONLINE:
		if (event == SM_EVENT_AUTH_SUCCESS) {
			sm->last_success_time = (uint64_t)time(NULL);
			set_error(sm, "");
		} else if (event == SM_EVENT_AUTH_FAILURE ||
			   event == SM_EVENT_AUTH_TIMEOUT ||
			   event == SM_EVENT_LINK_DOWN ||
			   event == SM_EVENT_HEALTH_FAILED) {
			set_error(sm, sm_event_to_string(event));
			transition_to(sm, STATE_RECOVERING);
			sm->retry_count++;
			schedule_retry(sm, now_ms);
		}
		break;
	case STATE_AUTH_FAILED:
	case STATE_RECOVERING:
		if (event == SM_EVENT_RETRY_TIMER)
			transition_to(sm, STATE_WAIT_LINK);
		break;
	case STATE_STOPPED:
		break;
	default:
		return -1;
	}

	return 0;
}

int state_machine_tick(state_machine_t *sm, uint64_t now_ms)
{
	auth_backend_event_t backend_event;
	dhcp_status_t dhcp_status;

	switch (sm->state) {
	case STATE_LOAD_CONFIG:
		return state_machine_dispatch(sm, SM_EVENT_CONFIG_LOADED);
	case STATE_WAIT_LINK:
		if (sm->deps.dry_run || netif_is_link_up(sm->deps.config->device))
			return state_machine_dispatch(sm, SM_EVENT_LINK_UP);
		break;
	case STATE_PREPARE_INTERFACE:
		return state_machine_dispatch(sm, SM_EVENT_INTERFACE_READY);
	case STATE_AUTHENTICATING:
		if (now_ms - sm->auth_started_at_ms >
		    (uint64_t)sm->deps.config->auth_timeout_sec * 1000ULL)
			return state_machine_dispatch(sm, SM_EVENT_AUTH_TIMEOUT);

		if (sm->deps.backend != NULL && sm->deps.backend->tick != NULL) {
			backend_event = sm->deps.backend->tick(sm->deps.backend, now_ms);
			if (backend_event == AUTH_BACKEND_EVENT_SUCCESS)
				return state_machine_dispatch(sm, SM_EVENT_AUTH_SUCCESS);
			if (backend_event == AUTH_BACKEND_EVENT_FAILURE)
				return state_machine_dispatch(sm, SM_EVENT_AUTH_FAILURE);
			if (backend_event == AUTH_BACKEND_EVENT_TIMEOUT)
				return state_machine_dispatch(sm, SM_EVENT_AUTH_TIMEOUT);
		}
		break;
	case STATE_AUTH_SUCCESS:
		if (!sm->deps.config->dhcp_after_success)
			return state_machine_dispatch(sm, SM_EVENT_DHCP_SUCCESS);
		transition_to(sm, STATE_DHCP_RENEWING);
		break;
	case STATE_DHCP_RENEWING:
		if (!sm->dhcp_renew_requested) {
			sm->dhcp_renew_requested = true;
			if (dhcp_renew(sm->deps.config->interface, sm->deps.dry_run) != 0)
				return state_machine_dispatch(sm, SM_EVENT_DHCP_FAILURE);
			if (sm->deps.dry_run)
				return state_machine_dispatch(sm, SM_EVENT_DHCP_SUCCESS);
		}

		if (dhcp_get_status(sm->deps.config->interface, &dhcp_status) == 0 &&
		    dhcp_is_online(&dhcp_status)) {
			LOG_INFO("DHCP online: ip=%s gateway=%s dns=%s",
				 dhcp_status.ip,
				 dhcp_status.gateway[0] != '\0' ? dhcp_status.gateway : "(none)",
				 dhcp_status.dns[0] != '\0' ? dhcp_status.dns : "(none)");
			return state_machine_dispatch(sm, SM_EVENT_DHCP_SUCCESS);
		}

		if (now_ms - sm->dhcp_started_at_ms >
		    (uint64_t)sm->deps.config->dhcp_timeout_sec * 1000ULL)
			return state_machine_dispatch(sm, SM_EVENT_DHCP_FAILURE);
		break;
	case STATE_ONLINE:
		if (sm->deps.config->reauth_enable &&
		    sm->deps.backend != NULL && sm->deps.backend->tick != NULL) {
			backend_event = sm->deps.backend->tick(sm->deps.backend, now_ms);
			if (backend_event == AUTH_BACKEND_EVENT_SUCCESS)
				return state_machine_dispatch(sm, SM_EVENT_AUTH_SUCCESS);
			if (backend_event == AUTH_BACKEND_EVENT_FAILURE)
				return state_machine_dispatch(sm, SM_EVENT_AUTH_FAILURE);
			if (backend_event == AUTH_BACKEND_EVENT_TIMEOUT)
				return state_machine_dispatch(sm, SM_EVENT_AUTH_TIMEOUT);
		}

		if (sm->last_online_check_ms == 0 ||
		    now_ms - sm->last_online_check_ms >=
		    (uint64_t)sm->deps.config->online_check_interval_sec * 1000ULL) {
			sm->last_online_check_ms = now_ms;
			if (watchdog_check(sm->deps.config, sm->deps.dry_run) != 0)
				return state_machine_dispatch(sm, SM_EVENT_HEALTH_FAILED);
		}
		break;
	case STATE_AUTH_FAILED:
	case STATE_RECOVERING:
		if (now_ms >= sm->retry_after_ms)
			return state_machine_dispatch(sm, SM_EVENT_RETRY_TIMER);
		break;
	case STATE_INIT:
	case STATE_STOPPED:
	default:
		break;
	}

	return 0;
}

bool state_machine_is_stopped(const state_machine_t *sm)
{
	return sm == NULL || sm->state == STATE_STOPPED;
}
