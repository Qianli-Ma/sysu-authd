#include "watchdog.h"

#include "dhcp.h"
#include "netif.h"

int watchdog_check(const config_t *cfg, bool dry_run)
{
	dhcp_status_t dhcp_status;

	if (cfg == NULL)
		return -1;
	if (dry_run)
		return 0;
	if (!cfg->healthcheck_enable)
		return 0;
	if (cfg->device[0] != '\0' && !netif_is_link_up(cfg->device))
		return -1;
	if (cfg->dhcp_after_success &&
	    dhcp_get_status(cfg->interface, &dhcp_status) == 0 &&
	    !dhcp_is_online(&dhcp_status))
		return -1;

	return 0;
}
