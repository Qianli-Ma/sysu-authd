#include "backend_ruijie.h"

#include "backend_eapol.h"

int backend_ruijie_register(auth_backend_t *backend)
{
	/* SYSU/Ruijie compatibility currently shares the standard EAPOL/EAP-MD5
	 * flow. Vendor keepalive and compatibility fields should be added here as
	 * wrappers around the standard backend, not in the main state machine. */
	return backend_eapol_register_named(backend, "ruijie_compat");
}
