#ifndef SYSU_AUTHD_NETIF_H
#define SYSU_AUTHD_NETIF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NETIF_MAC_LEN 6U

int netif_resolve_device(const char *interface, char *device, size_t device_len);
bool netif_is_link_up(const char *device);
int netif_get_mac(const char *device, uint8_t mac[NETIF_MAC_LEN]);

#endif
