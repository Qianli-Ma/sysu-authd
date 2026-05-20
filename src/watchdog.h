#ifndef SYSU_AUTHD_WATCHDOG_H
#define SYSU_AUTHD_WATCHDOG_H

#include <stdbool.h>

#include "config.h"

int watchdog_check(const config_t *cfg, bool dry_run);

#endif
