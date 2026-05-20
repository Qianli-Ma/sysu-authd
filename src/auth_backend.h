#ifndef SYSU_AUTHD_AUTH_BACKEND_H
#define SYSU_AUTHD_AUTH_BACKEND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"
#include "profile.h"

typedef enum {
	AUTH_BACKEND_EVENT_NONE = 0,
	AUTH_BACKEND_EVENT_PROGRESS,
	AUTH_BACKEND_EVENT_SUCCESS,
	AUTH_BACKEND_EVENT_FAILURE,
	AUTH_BACKEND_EVENT_TIMEOUT
} auth_backend_event_t;

typedef enum {
	AUTH_BACKEND_STATUS_IDLE = 0,
	AUTH_BACKEND_STATUS_RUNNING,
	AUTH_BACKEND_STATUS_SUCCESS,
	AUTH_BACKEND_STATUS_FAILED,
	AUTH_BACKEND_STATUS_STOPPED
} auth_backend_status_t;

typedef struct auth_backend auth_backend_t;

typedef struct {
	const config_t *config;
	const profile_t *profile;
	const char *password;
	bool dry_run;
} auth_backend_init_args_t;

struct auth_backend {
	const char *name;
	void *ctx;
	int (*init)(auth_backend_t *backend, const auth_backend_init_args_t *args);
	int (*start)(auth_backend_t *backend);
	auth_backend_event_t (*handle_packet)(auth_backend_t *backend,
					      const uint8_t *packet,
					      size_t packet_len);
	auth_backend_event_t (*tick)(auth_backend_t *backend, uint64_t now_ms);
	void (*stop)(auth_backend_t *backend);
	auth_backend_status_t (*get_status)(auth_backend_t *backend);
	void (*destroy)(auth_backend_t *backend);
};

const char *auth_backend_event_to_string(auth_backend_event_t event);
const char *auth_backend_status_to_string(auth_backend_status_t status);

int auth_backend_create(const char *name, auth_backend_t *backend);
void auth_backend_destroy(auth_backend_t *backend);

int backend_eapol_register(auth_backend_t *backend);
int backend_ruijie_register(auth_backend_t *backend);

#endif
