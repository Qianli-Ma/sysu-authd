#include "eap.h"

#include "md5.h"

#include <arpa/inet.h>
#include <stdbool.h>
#include <string.h>

int eap_parse(const uint8_t *buf, size_t len, eap_packet_t *packet)
{
	uint16_t nlen;
	uint16_t plen;

	if (buf == NULL || packet == NULL || len < EAP_HEADER_LEN)
		return -1;

	memcpy(&nlen, buf + 2, sizeof(nlen));
	plen = ntohs(nlen);
	if (plen < EAP_HEADER_LEN || len < plen)
		return -1;

	memset(packet, 0, sizeof(*packet));
	packet->code = buf[0];
	packet->identifier = buf[1];
	packet->length = plen;

	if (packet->code == EAP_CODE_REQUEST || packet->code == EAP_CODE_RESPONSE) {
		if (plen < EAP_HEADER_LEN + 1U)
			return -1;
		packet->type = buf[4];
		packet->data = plen > EAP_HEADER_LEN + 1U ? buf + 5 : NULL;
		packet->data_len = plen - EAP_HEADER_LEN - 1U;
	}

	return 0;
}

eap_event_t eap_classify(const eap_packet_t *packet)
{
	if (packet == NULL)
		return EAP_EVENT_NONE;

	if (packet->code == EAP_CODE_SUCCESS)
		return EAP_EVENT_SUCCESS;
	if (packet->code == EAP_CODE_FAILURE)
		return EAP_EVENT_FAILURE;
	if (packet->code != EAP_CODE_REQUEST)
		return EAP_EVENT_UNSUPPORTED;
	if (packet->type == EAP_TYPE_IDENTITY)
		return EAP_EVENT_REQUEST_IDENTITY;
	if (packet->type == EAP_TYPE_MD5_CHALLENGE)
		return EAP_EVENT_REQUEST_CHALLENGE;

	return EAP_EVENT_UNSUPPORTED;
}

int eap_format_identity(char *dst, size_t dst_len,
			const char *format, const char *username)
{
	const char *p;
	size_t used = 0;
	bool replaced = false;

	if (dst == NULL || dst_len == 0 || username == NULL)
		return -1;

	if (format == NULL || *format == '\0')
		format = "%u";

	for (p = format; *p != '\0'; p++) {
		const char *piece = NULL;
		char literal[2] = {*p, '\0'};
		size_t piece_len;

		if (*p == '%') {
			p++;
			if (*p == '\0')
				return -1;
			if (*p == 'u') {
				piece = username;
				replaced = true;
			} else if (*p == '%') {
				piece = "%";
			} else {
				return -1;
			}
		} else {
			piece = literal;
		}

		piece_len = strlen(piece);
		if (used + piece_len >= dst_len)
			return -1;
		memcpy(dst + used, piece, piece_len);
		used += piece_len;
	}

	if (!replaced)
		return -1;

	dst[used] = '\0';
	return 0;
}

int eap_get_md5_challenge(const eap_packet_t *packet,
			  const uint8_t **challenge, size_t *challenge_len)
{
	uint8_t value_size;

	if (packet == NULL || challenge == NULL || challenge_len == NULL)
		return -1;
	if (packet->code != EAP_CODE_REQUEST ||
	    packet->type != EAP_TYPE_MD5_CHALLENGE)
		return -1;
	if (packet->data == NULL || packet->data_len < 1U)
		return -1;

	value_size = packet->data[0];
	if (value_size == 0 || packet->data_len < 1U + (size_t)value_size)
		return -1;

	*challenge = packet->data + 1U;
	*challenge_len = value_size;
	return 0;
}

int eap_build_identity_response(uint8_t identifier, const char *identity,
				uint8_t *buf, size_t buf_len, size_t *out_len)
{
	size_t identity_len;
	size_t packet_len;
	uint16_t nlen;

	if (identity == NULL || buf == NULL || out_len == NULL)
		return -1;

	identity_len = strlen(identity);
	packet_len = EAP_HEADER_LEN + 1U + identity_len;
	if (identity_len > 1024U || buf_len < packet_len)
		return -1;

	buf[0] = EAP_CODE_RESPONSE;
	buf[1] = identifier;
	nlen = htons((uint16_t)packet_len);
	memcpy(buf + 2, &nlen, sizeof(nlen));
	buf[4] = EAP_TYPE_IDENTITY;
	memcpy(buf + 5, identity, identity_len);

	*out_len = packet_len;
	return 0;
}

int eap_build_md5_challenge_response(uint8_t identifier, const char *password,
				     const uint8_t *challenge, size_t challenge_len,
				     const char *identity, uint8_t *buf,
				     size_t buf_len, size_t *out_len)
{
	md5_ctx_t md5;
	uint8_t digest[MD5_DIGEST_LEN];
	size_t password_len;
	size_t identity_len;
	size_t packet_len;
	uint16_t nlen;

	if (password == NULL || challenge == NULL || identity == NULL ||
	    buf == NULL || out_len == NULL)
		return -1;
	if (challenge_len == 0 || challenge_len > 255U)
		return -1;

	password_len = strlen(password);
	identity_len = strlen(identity);
	packet_len = EAP_HEADER_LEN + 1U + 1U + EAP_MD5_VALUE_SIZE + identity_len;
	if (identity_len > 1024U || buf_len < packet_len)
		return -1;

	md5_init(&md5);
	md5_update(&md5, &identifier, sizeof(identifier));
	md5_update(&md5, (const uint8_t *)password, password_len);
	md5_update(&md5, challenge, challenge_len);
	md5_final(&md5, digest);

	buf[0] = EAP_CODE_RESPONSE;
	buf[1] = identifier;
	nlen = htons((uint16_t)packet_len);
	memcpy(buf + 2, &nlen, sizeof(nlen));
	buf[4] = EAP_TYPE_MD5_CHALLENGE;
	buf[5] = EAP_MD5_VALUE_SIZE;
	memcpy(buf + 6, digest, sizeof(digest));
	memcpy(buf + 6 + sizeof(digest), identity, identity_len);

	memset(digest, 0, sizeof(digest));
	*out_len = packet_len;
	return 0;
}
