#include "netif.h"

#include "utils.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

int netif_resolve_device(const char *interface, char *device, size_t device_len)
{
	if (device == NULL || device_len == 0)
		return -1;
	if (interface == NULL || *interface == '\0')
		return safe_strcpy(device, device_len, "");

	/* First-stage fallback: use interface name as device name. OpenWrt UCI
	 * resolution is intentionally kept out of this portable module for now. */
	return safe_strcpy(device, device_len, interface);
}

bool netif_is_link_up(const char *device)
{
	char path[256];
	FILE *fp;
	char value[16];

	if (device == NULL || *device == '\0')
		return false;

	if (snprintf(path, sizeof(path), "/sys/class/net/%s/carrier", device) >=
	    (int)sizeof(path))
		return false;

	fp = fopen(path, "r");
	if (fp == NULL)
		return errno == ENOENT ? false : false;

	if (fgets(value, sizeof(value), fp) == NULL) {
		fclose(fp);
		return false;
	}
	fclose(fp);

	return value[0] == '1';
}

int netif_get_mac(const char *device, uint8_t mac[NETIF_MAC_LEN])
{
	char path[256];
	FILE *fp;
	unsigned int bytes[NETIF_MAC_LEN];
	int matched;
	size_t i;

	if (device == NULL || mac == NULL || *device == '\0')
		return -1;

	if (snprintf(path, sizeof(path), "/sys/class/net/%s/address", device) >=
	    (int)sizeof(path))
		return -1;

	fp = fopen(path, "r");
	if (fp == NULL)
		return -1;

	matched = fscanf(fp, "%02x:%02x:%02x:%02x:%02x:%02x",
			 &bytes[0], &bytes[1], &bytes[2],
			 &bytes[3], &bytes[4], &bytes[5]);
	fclose(fp);

	if (matched != (int)NETIF_MAC_LEN)
		return -1;

	for (i = 0; i < NETIF_MAC_LEN; i++)
		mac[i] = (uint8_t)bytes[i];

	return 0;
}
