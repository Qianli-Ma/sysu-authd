#define _GNU_SOURCE

#include "raw_socket.h"

#include "eapol.h"
#include "netif.h"
#include "utils.h"

#include <arpa/inet.h>
#include <errno.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static const uint8_t g_eapol_pae_group_addr[RAW_SOCKET_MAC_LEN] = {
	0x01, 0x80, 0xc2, 0x00, 0x00, 0x03
};

static raw_packet_type_t packet_type_from_ll(int pkttype)
{
	switch (pkttype) {
	case PACKET_HOST:
		return RAW_PACKET_HOST;
	case PACKET_BROADCAST:
		return RAW_PACKET_BROADCAST;
	case PACKET_MULTICAST:
		return RAW_PACKET_MULTICAST;
	case PACKET_OTHERHOST:
		return RAW_PACKET_OTHERHOST;
	case PACKET_OUTGOING:
		return RAW_PACKET_OUTGOING;
	default:
		return RAW_PACKET_UNKNOWN;
	}
}

int raw_socket_open(raw_socket_t *sock, const char *device)
{
	struct sockaddr_ll addr;
	int fd;
	unsigned int ifindex;

	if (sock == NULL || device == NULL || *device == '\0')
		return -1;

	memset(sock, 0, sizeof(*sock));
	sock->fd = -1;

	fd = socket(AF_PACKET, SOCK_RAW | SOCK_CLOEXEC, htons(EAPOL_ETHERTYPE));
	if (fd < 0)
		return -1;

	ifindex = if_nametoindex(device);
	if (ifindex == 0) {
		close(fd);
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sll_family = AF_PACKET;
	addr.sll_protocol = htons(EAPOL_ETHERTYPE);
	addr.sll_ifindex = (int)ifindex;

	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		close(fd);
		return -1;
	}

	if (netif_get_mac(device, sock->mac) != 0) {
		close(fd);
		return -1;
	}

	sock->fd = fd;
	sock->ifindex = (int)ifindex;
	(void)safe_strcpy(sock->device, sizeof(sock->device), device);
	return 0;
}

int raw_socket_send_frame(raw_socket_t *sock,
			  const uint8_t dest_mac[RAW_SOCKET_MAC_LEN],
			  const uint8_t *payload, size_t payload_len)
{
	struct sockaddr_ll addr;
	uint8_t frame[ETH_FRAME_LEN];
	uint16_t ethertype;
	ssize_t sent;

	if (sock == NULL || sock->fd < 0 || dest_mac == NULL ||
	    payload == NULL || payload_len == 0)
		return -1;
	if (payload_len > sizeof(frame) - ETH_HLEN)
		return -1;

	memcpy(frame, dest_mac, RAW_SOCKET_MAC_LEN);
	memcpy(frame + RAW_SOCKET_MAC_LEN, sock->mac, RAW_SOCKET_MAC_LEN);
	ethertype = htons(EAPOL_ETHERTYPE);
	memcpy(frame + 12U, &ethertype, sizeof(ethertype));
	memcpy(frame + ETH_HLEN, payload, payload_len);

	memset(&addr, 0, sizeof(addr));
	addr.sll_family = AF_PACKET;
	addr.sll_protocol = htons(EAPOL_ETHERTYPE);
	addr.sll_ifindex = sock->ifindex;
	addr.sll_halen = RAW_SOCKET_MAC_LEN;
	memcpy(addr.sll_addr, dest_mac, RAW_SOCKET_MAC_LEN);

	sent = sendto(sock->fd, frame, ETH_HLEN + payload_len, 0,
		      (struct sockaddr *)&addr, sizeof(addr));
	if (sent < 0)
		return -1;

	return (size_t)sent == ETH_HLEN + payload_len ? 0 : -1;
}

int raw_socket_send_eapol(raw_socket_t *sock, const uint8_t *payload,
			  size_t payload_len)
{
	return raw_socket_send_frame(sock, g_eapol_pae_group_addr, payload,
				     payload_len);
}

int raw_socket_recv_frame(raw_socket_t *sock, uint8_t *buf, size_t buf_len,
			  size_t *out_len,
			  uint8_t src_mac[RAW_SOCKET_MAC_LEN],
			  raw_packet_type_t *packet_type)
{
	struct sockaddr_ll addr;
	socklen_t addr_len = sizeof(addr);
	uint8_t frame[ETH_FRAME_LEN];
	uint16_t ethertype;
	ssize_t got;

	if (sock == NULL || sock->fd < 0 || buf == NULL || out_len == NULL)
		return -1;

	memset(&addr, 0, sizeof(addr));
	got = recvfrom(sock->fd, frame, sizeof(frame), MSG_DONTWAIT,
		       (struct sockaddr *)&addr, &addr_len);
	if (got < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			*out_len = 0;
			return 0;
		}
		return -1;
	}

	if (got < ETH_HLEN) {
		*out_len = 0;
		return 0;
	}

	if (packet_type != NULL)
		*packet_type = packet_type_from_ll(addr.sll_pkttype);

	memcpy(&ethertype, frame + 12U, sizeof(ethertype));
	if (ntohs(ethertype) != EAPOL_ETHERTYPE) {
		*out_len = 0;
		return 0;
	}

	if ((size_t)got - ETH_HLEN > buf_len)
		return -1;

	if (src_mac != NULL)
		memcpy(src_mac, frame + RAW_SOCKET_MAC_LEN, RAW_SOCKET_MAC_LEN);
	memcpy(buf, frame + ETH_HLEN, (size_t)got - ETH_HLEN);
	*out_len = (size_t)got - ETH_HLEN;
	return 0;
}

void raw_socket_close(raw_socket_t *sock)
{
	if (sock == NULL)
		return;

	if (sock->fd >= 0)
		close(sock->fd);
	sock->fd = -1;
}
