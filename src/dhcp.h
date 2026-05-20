#ifndef SYSU_AUTHD_DHCP_H
#define SYSU_AUTHD_DHCP_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
	bool has_ipv4;
	bool has_default_route;
	char ip[64];
	char gateway[64];
	char dns[128];
} dhcp_status_t;

int dhcp_renew(const char *interface, bool dry_run);
int dhcp_get_wan_ip(const char *interface, char *buf, size_t buf_len);
int dhcp_get_status(const char *interface, dhcp_status_t *status);
bool dhcp_is_online(const dhcp_status_t *status);

#endif
