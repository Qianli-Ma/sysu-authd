#ifndef SYSU_AUTHD_EAPOL_H
#define SYSU_AUTHD_EAPOL_H

#include <stddef.h>
#include <stdint.h>

#define EAPOL_ETHERTYPE 0x888eU
#define EAPOL_MAX_PAYLOAD 1500U
#define EAPOL_HEADER_LEN 4U

typedef enum {
	EAPOL_TYPE_EAP_PACKET = 0,
	EAPOL_TYPE_START = 1,
	EAPOL_TYPE_LOGOFF = 2,
	EAPOL_TYPE_KEY = 3,
	EAPOL_TYPE_ASF_ALERT = 4
} eapol_type_t;

typedef struct {
	uint8_t version;
	uint8_t type;
	uint16_t length;
	const uint8_t *payload;
} eapol_frame_t;

int eapol_parse(const uint8_t *buf, size_t len, eapol_frame_t *frame);
int eapol_build_start(uint8_t version, uint8_t *buf, size_t buf_len, size_t *out_len);
int eapol_build_logoff(uint8_t version, uint8_t *buf, size_t buf_len, size_t *out_len);
int eapol_build_packet(uint8_t version, const uint8_t *payload, size_t payload_len,
		       uint8_t *buf, size_t buf_len, size_t *out_len);

#endif
