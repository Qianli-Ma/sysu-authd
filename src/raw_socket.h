#ifndef SYSU_AUTHD_RAW_SOCKET_H
#define SYSU_AUTHD_RAW_SOCKET_H

#include <stddef.h>
#include <stdint.h>

#define RAW_SOCKET_MAC_LEN 6U

typedef enum {
	RAW_PACKET_HOST = 0,
	RAW_PACKET_BROADCAST,
	RAW_PACKET_MULTICAST,
	RAW_PACKET_OTHERHOST,
	RAW_PACKET_OUTGOING,
	RAW_PACKET_UNKNOWN
} raw_packet_type_t;

typedef struct {
	int fd;
	char device[128];
	int ifindex;
	uint8_t mac[RAW_SOCKET_MAC_LEN];
} raw_socket_t;

int raw_socket_open(raw_socket_t *sock, const char *device);
int raw_socket_send_frame(raw_socket_t *sock,
			  const uint8_t dest_mac[RAW_SOCKET_MAC_LEN],
			  const uint8_t *payload, size_t payload_len);
int raw_socket_send_eapol(raw_socket_t *sock, const uint8_t *payload,
			  size_t payload_len);
int raw_socket_recv_frame(raw_socket_t *sock, uint8_t *buf, size_t buf_len,
			  size_t *out_len,
			  uint8_t src_mac[RAW_SOCKET_MAC_LEN],
			  raw_packet_type_t *packet_type);
void raw_socket_close(raw_socket_t *sock);

#endif
