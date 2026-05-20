#ifndef SYSU_AUTHD_EAP_H
#define SYSU_AUTHD_EAP_H

#include <stddef.h>
#include <stdint.h>

#define EAP_HEADER_LEN 4U
#define EAP_MD5_VALUE_SIZE 16U

typedef enum {
	EAP_CODE_REQUEST = 1,
	EAP_CODE_RESPONSE = 2,
	EAP_CODE_SUCCESS = 3,
	EAP_CODE_FAILURE = 4
} eap_code_t;

typedef enum {
	EAP_TYPE_IDENTITY = 1,
	EAP_TYPE_NOTIFICATION = 2,
	EAP_TYPE_NAK = 3,
	EAP_TYPE_MD5_CHALLENGE = 4
} eap_type_t;

typedef enum {
	EAP_EVENT_NONE = 0,
	EAP_EVENT_REQUEST_IDENTITY,
	EAP_EVENT_REQUEST_CHALLENGE,
	EAP_EVENT_SUCCESS,
	EAP_EVENT_FAILURE,
	EAP_EVENT_UNSUPPORTED
} eap_event_t;

typedef struct {
	uint8_t code;
	uint8_t identifier;
	uint16_t length;
	uint8_t type;
	const uint8_t *data;
	size_t data_len;
} eap_packet_t;

int eap_parse(const uint8_t *buf, size_t len, eap_packet_t *packet);
eap_event_t eap_classify(const eap_packet_t *packet);
int eap_format_identity(char *dst, size_t dst_len,
			const char *format, const char *username);
int eap_get_md5_challenge(const eap_packet_t *packet,
			  const uint8_t **challenge, size_t *challenge_len);
int eap_build_identity_response(uint8_t identifier, const char *identity,
				uint8_t *buf, size_t buf_len, size_t *out_len);
int eap_build_md5_challenge_response(uint8_t identifier, const char *password,
				     const uint8_t *challenge, size_t challenge_len,
				     const char *identity, uint8_t *buf,
				     size_t buf_len, size_t *out_len);

#endif
