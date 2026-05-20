#include "auth_backend.h"

#include <string.h>

const char *auth_backend_event_to_string(auth_backend_event_t event)
{
	switch (event) {
	case AUTH_BACKEND_EVENT_NONE:
		return "none";
	case AUTH_BACKEND_EVENT_PROGRESS:
		return "progress";
	case AUTH_BACKEND_EVENT_SUCCESS:
		return "success";
	case AUTH_BACKEND_EVENT_FAILURE:
		return "failure";
	case AUTH_BACKEND_EVENT_TIMEOUT:
		return "timeout";
	default:
		return "unknown";
	}
}

const char *auth_backend_status_to_string(auth_backend_status_t status)
{
	switch (status) {
	case AUTH_BACKEND_STATUS_IDLE:
		return "idle";
	case AUTH_BACKEND_STATUS_RUNNING:
		return "running";
	case AUTH_BACKEND_STATUS_SUCCESS:
		return "success";
	case AUTH_BACKEND_STATUS_FAILED:
		return "failed";
	case AUTH_BACKEND_STATUS_STOPPED:
		return "stopped";
	default:
		return "unknown";
	}
}

int auth_backend_create(const char *name, auth_backend_t *backend)
{
	if (backend == NULL || name == NULL)
		return -1;

	memset(backend, 0, sizeof(*backend));

	if (strcmp(name, "standard_eapol") == 0)
		return backend_eapol_register(backend);
	if (strcmp(name, "ruijie_compat") == 0)
		return backend_ruijie_register(backend);

	return -1;
}

void auth_backend_destroy(auth_backend_t *backend)
{
	if (backend == NULL)
		return;

	if (backend->destroy != NULL)
		backend->destroy(backend);
	memset(backend, 0, sizeof(*backend));
}
