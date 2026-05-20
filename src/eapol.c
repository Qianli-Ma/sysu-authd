#include "eapol.h"

#include <arpa/inet.h>
#include <string.h>

static int build_header(uint8_t version, uint8_t type, const uint8_t *payload,
			size_t payload_len, uint8_t *buf, size_t buf_len,
			size_t *out_len)
{
	uint16_t nlen;

	if (buf == NULL || out_len == NULL)
		return -1;
	if (payload_len > EAPOL_MAX_PAYLOAD ||
	    buf_len < EAPOL_HEADER_LEN + payload_len)
		return -1;
	if (payload_len > 0 && payload == NULL)
		return -1;

	buf[0] = version;
	buf[1] = type;
	nlen = htons((uint16_t)payload_len);
	memcpy(buf + 2, &nlen, sizeof(nlen));

	if (payload_len > 0)
		memcpy(buf + EAPOL_HEADER_LEN, payload, payload_len);

	*out_len = EAPOL_HEADER_LEN + payload_len;
	return 0;
}

int eapol_parse(const uint8_t *buf, size_t len, eapol_frame_t *frame)
{
	uint16_t nlen;
	uint16_t payload_len;

	if (buf == NULL || frame == NULL || len < EAPOL_HEADER_LEN)
		return -1;

	memcpy(&nlen, buf + 2, sizeof(nlen));
	payload_len = ntohs(nlen);

	if (payload_len > EAPOL_MAX_PAYLOAD)
		return -1;
	if (len < EAPOL_HEADER_LEN + (size_t)payload_len)
		return -1;

	frame->version = buf[0];
	frame->type = buf[1];
	frame->length = payload_len;
	frame->payload = payload_len > 0 ? buf + EAPOL_HEADER_LEN : NULL;
	return 0;
}

int eapol_build_start(uint8_t version, uint8_t *buf, size_t buf_len, size_t *out_len)
{
	return build_header(version, EAPOL_TYPE_START, NULL, 0, buf, buf_len, out_len);
}

int eapol_build_logoff(uint8_t version, uint8_t *buf, size_t buf_len, size_t *out_len)
{
	return build_header(version, EAPOL_TYPE_LOGOFF, NULL, 0, buf, buf_len, out_len);
}

int eapol_build_packet(uint8_t version, const uint8_t *payload, size_t payload_len,
		       uint8_t *buf, size_t buf_len, size_t *out_len)
{
	return build_header(version, EAPOL_TYPE_EAP_PACKET, payload, payload_len,
			    buf, buf_len, out_len);
}
