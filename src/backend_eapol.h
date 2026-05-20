#ifndef SYSU_AUTHD_BACKEND_EAPOL_H
#define SYSU_AUTHD_BACKEND_EAPOL_H

#include "auth_backend.h"

int backend_eapol_register(auth_backend_t *backend);
int backend_eapol_register_named(auth_backend_t *backend, const char *name);

#endif
