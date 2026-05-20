#include "backend_eapol.h"

#include "eap.h"
#include "eapol.h"
#include "log.h"
#include "raw_socket.h"
#include "utils.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BACKEND_RX_BUF_LEN 2048U
#define BACKEND_EAP_BUF_LEN 1500U
#define BACKEND_EAPOL_BUF_LEN 1600U
#define EAPOL_START_INTERVAL_MS 3000U

typedef enum {
	EAPOL_SESSION_IDLE = 0,
	EAPOL_SESSION_WAIT_REQUEST,
	EAPOL_SESSION_SENT_IDENTITY,
	EAPOL_SESSION_SENT_CHALLENGE,
	EAPOL_SESSION_DONE
} eapol_session_phase_t;

typedef struct {
	auth_backend_status_t status;
	eapol_session_phase_t phase;
	uint64_t started_at_ms;
	uint64_t last_start_ms;
	uint64_t last_wait_log_ms;
	unsigned int start_count;
	bool dry_run;
	bool socket_open;
	bool logoff_on_stop;
	bool reauth_enable;
	bool authenticated;
	raw_socket_t sock;
	uint8_t eapol_version;
	char username[SYSU_AUTHD_MAX_STR];
	char identity[SYSU_AUTHD_MAX_STR];
	char password[SYSU_AUTHD_PASSWORD_MAX];
	char device[SYSU_AUTHD_MAX_STR];
	char backend_name[SYSU_AUTHD_MAX_STR];
} backend_eapol_ctx_t;

static void eapol_destroy(auth_backend_t *backend);

static const char *phase_to_string(eapol_session_phase_t phase)
{
	switch (phase) {
	case EAPOL_SESSION_IDLE:
		return "idle";
	case EAPOL_SESSION_WAIT_REQUEST:
		return "wait-request";
	case EAPOL_SESSION_SENT_IDENTITY:
		return "sent-identity";
	case EAPOL_SESSION_SENT_CHALLENGE:
		return "sent-challenge";
	case EAPOL_SESSION_DONE:
		return "done";
	default:
		return "unknown";
	}
}

static int send_eap_payload(backend_eapol_ctx_t *ctx, const uint8_t *eap,
			    size_t eap_len)
{
	uint8_t eapol[BACKEND_EAPOL_BUF_LEN];
	size_t eapol_len = 0;

	if (ctx->dry_run) {
		LOG_DEBUG("dry-run: %s would send EAP payload len=%zu",
			  ctx->backend_name, eap_len);
		return 0;
	}

	if (eapol_build_packet(ctx->eapol_version, eap, eap_len, eapol,
			       sizeof(eapol), &eapol_len) != 0)
		return -1;

	return raw_socket_send_eapol(&ctx->sock, eapol, eapol_len);
}

static int send_start(backend_eapol_ctx_t *ctx)
{
	uint8_t buf[EAPOL_HEADER_LEN];
	size_t len = 0;

	if (ctx->dry_run) {
		ctx->last_start_ms = now_msec();
		ctx->start_count++;
		LOG_INFO("dry-run: %s would send EAPOL-Start on %s",
			 ctx->backend_name, ctx->device);
		return 0;
	}

	if (eapol_build_start(ctx->eapol_version, buf, sizeof(buf), &len) != 0)
		return -1;

	if (raw_socket_send_eapol(&ctx->sock, buf, len) != 0)
		return -1;

	ctx->last_start_ms = now_msec();
	ctx->start_count++;
	LOG_INFO("%s sent EAPOL-Start on %s", ctx->backend_name, ctx->device);
	return 0;
}

static auth_backend_event_t handle_eap_packet(backend_eapol_ctx_t *ctx,
					      const eap_packet_t *packet)
{
	uint8_t response[BACKEND_EAP_BUF_LEN];
	const uint8_t *challenge = NULL;
	size_t challenge_len = 0;
	size_t response_len = 0;
	eap_event_t event;

	event = eap_classify(packet);
	switch (event) {
	case EAP_EVENT_REQUEST_IDENTITY:
		if (eap_build_identity_response(packet->identifier, ctx->identity,
						response, sizeof(response),
						&response_len) != 0) {
			LOG_ERROR("failed to build EAP identity response");
			ctx->status = AUTH_BACKEND_STATUS_FAILED;
			return AUTH_BACKEND_EVENT_FAILURE;
		}
		if (send_eap_payload(ctx, response, response_len) != 0) {
			LOG_ERROR("failed to send EAP identity response");
			ctx->status = AUTH_BACKEND_STATUS_FAILED;
			return AUTH_BACKEND_EVENT_FAILURE;
		}
		ctx->phase = EAPOL_SESSION_SENT_IDENTITY;
		LOG_INFO("%s sent EAP identity response", ctx->backend_name);
		return AUTH_BACKEND_EVENT_PROGRESS;
	case EAP_EVENT_REQUEST_CHALLENGE:
		if (eap_get_md5_challenge(packet, &challenge, &challenge_len) != 0) {
			LOG_ERROR("invalid EAP-MD5 challenge");
			ctx->status = AUTH_BACKEND_STATUS_FAILED;
			return AUTH_BACKEND_EVENT_FAILURE;
		}
		if (eap_build_md5_challenge_response(packet->identifier,
						    ctx->password,
						    challenge, challenge_len,
						    ctx->username,
						    response, sizeof(response),
						    &response_len) != 0) {
			LOG_ERROR("failed to build EAP-MD5 challenge response");
			ctx->status = AUTH_BACKEND_STATUS_FAILED;
			return AUTH_BACKEND_EVENT_FAILURE;
		}
		if (send_eap_payload(ctx, response, response_len) != 0) {
			LOG_ERROR("failed to send EAP-MD5 challenge response");
			ctx->status = AUTH_BACKEND_STATUS_FAILED;
			return AUTH_BACKEND_EVENT_FAILURE;
		}
		ctx->phase = EAPOL_SESSION_SENT_CHALLENGE;
		LOG_INFO("%s sent EAP-MD5 challenge response", ctx->backend_name);
		return AUTH_BACKEND_EVENT_PROGRESS;
	case EAP_EVENT_SUCCESS:
		ctx->status = AUTH_BACKEND_STATUS_RUNNING;
		ctx->phase = EAPOL_SESSION_DONE;
		ctx->authenticated = true;
		LOG_INFO("%s received EAP success", ctx->backend_name);
		return AUTH_BACKEND_EVENT_SUCCESS;
	case EAP_EVENT_FAILURE:
		ctx->status = AUTH_BACKEND_STATUS_FAILED;
		ctx->phase = EAPOL_SESSION_DONE;
		LOG_WARN("%s received EAP failure", ctx->backend_name);
		return AUTH_BACKEND_EVENT_FAILURE;
	case EAP_EVENT_UNSUPPORTED:
		LOG_WARN("%s received unsupported EAP code=%u type=%u",
			 ctx->backend_name, packet->code, packet->type);
		return AUTH_BACKEND_EVENT_PROGRESS;
	case EAP_EVENT_NONE:
	default:
		return AUTH_BACKEND_EVENT_NONE;
	}
}

static auth_backend_event_t process_eapol_payload(backend_eapol_ctx_t *ctx,
						  const uint8_t *payload,
						  size_t payload_len)
{
	eapol_frame_t frame;
	eap_packet_t packet;

	if (eapol_parse(payload, payload_len, &frame) != 0) {
		LOG_WARN("%s ignored malformed EAPOL frame", ctx->backend_name);
		return AUTH_BACKEND_EVENT_NONE;
	}

	if (frame.type != EAPOL_TYPE_EAP_PACKET)
		return AUTH_BACKEND_EVENT_NONE;

	if (frame.payload == NULL || frame.length == 0)
		return AUTH_BACKEND_EVENT_NONE;

	if (eap_parse(frame.payload, frame.length, &packet) != 0) {
		LOG_WARN("%s ignored malformed EAP packet", ctx->backend_name);
		return AUTH_BACKEND_EVENT_NONE;
	}

	return handle_eap_packet(ctx, &packet);
}

static int eapol_init(auth_backend_t *backend, const auth_backend_init_args_t *args)
{
	backend_eapol_ctx_t *ctx;

	if (backend == NULL || args == NULL || args->config == NULL ||
	    args->profile == NULL)
		return -1;

	ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL)
		return -1;

	ctx->status = AUTH_BACKEND_STATUS_IDLE;
	ctx->phase = EAPOL_SESSION_IDLE;
	ctx->dry_run = args->dry_run;
	ctx->sock.fd = -1;
	ctx->eapol_version = (uint8_t)args->config->eapol_version;
	if (ctx->eapol_version == 0)
		ctx->eapol_version = args->profile->eapol_version;
	ctx->logoff_on_stop = args->config->logoff_on_stop;
	ctx->reauth_enable = args->config->reauth_enable;
	(void)safe_strcpy(ctx->backend_name, sizeof(ctx->backend_name),
			  backend->name != NULL ? backend->name : "standard_eapol");
	(void)safe_strcpy(ctx->username, sizeof(ctx->username),
			  args->config->username);
	if (eap_format_identity(ctx->identity, sizeof(ctx->identity),
				args->config->identity_format,
				args->config->username) != 0) {
		LOG_ERROR("invalid identity_format for %s backend", ctx->backend_name);
		eapol_destroy(backend);
		return -1;
	}
	(void)safe_strcpy(ctx->password, sizeof(ctx->password), args->password);
	(void)safe_strcpy(ctx->device, sizeof(ctx->device), args->config->device);
	backend->ctx = ctx;

	if (ctx->username[0] == '\0') {
		LOG_ERROR("username is required for %s backend", ctx->backend_name);
		eapol_destroy(backend);
		return -1;
	}

	if (ctx->password[0] == '\0') {
		LOG_ERROR("password is required for %s backend", ctx->backend_name);
		eapol_destroy(backend);
		return -1;
	}

	if (ctx->device[0] == '\0') {
		LOG_ERROR("device is required for %s backend", ctx->backend_name);
		eapol_destroy(backend);
		return -1;
	}

	LOG_DEBUG("%s backend initialized for user %s identity %s",
		  ctx->backend_name, ctx->username, ctx->identity);
	return 0;
}

static int eapol_start(auth_backend_t *backend)
{
	backend_eapol_ctx_t *ctx = backend->ctx;

	if (ctx == NULL)
		return -1;

	ctx->status = AUTH_BACKEND_STATUS_RUNNING;
	ctx->phase = EAPOL_SESSION_WAIT_REQUEST;
	ctx->started_at_ms = now_msec();
	ctx->last_start_ms = 0;
	ctx->last_wait_log_ms = 0;
	ctx->start_count = 0;

	if (ctx->dry_run) {
		LOG_INFO("dry-run: %s backend simulates EAPOL authentication",
			 ctx->backend_name);
		return 0;
	}

	if (raw_socket_open(&ctx->sock, ctx->device) != 0) {
		LOG_ERROR("%s failed to open EAPOL raw socket on %s",
			  ctx->backend_name, ctx->device);
		ctx->status = AUTH_BACKEND_STATUS_FAILED;
		return -1;
	}

	ctx->socket_open = true;
	if (send_start(ctx) != 0) {
		LOG_ERROR("%s failed to send EAPOL-Start", ctx->backend_name);
		ctx->status = AUTH_BACKEND_STATUS_FAILED;
		return -1;
	}

	return 0;
}

static auth_backend_event_t eapol_handle_packet(auth_backend_t *backend,
						const uint8_t *packet,
						size_t packet_len)
{
	backend_eapol_ctx_t *ctx = backend->ctx;

	if (ctx == NULL || ctx->status != AUTH_BACKEND_STATUS_RUNNING)
		return AUTH_BACKEND_EVENT_NONE;
	if (packet == NULL || packet_len == 0)
		return AUTH_BACKEND_EVENT_NONE;

	return process_eapol_payload(ctx, packet, packet_len);
}

static auth_backend_event_t eapol_tick(auth_backend_t *backend, uint64_t now_ms)
{
	backend_eapol_ctx_t *ctx = backend->ctx;
	auth_backend_event_t event;
	unsigned int packets = 0;

	if (ctx == NULL || ctx->status != AUTH_BACKEND_STATUS_RUNNING)
		return AUTH_BACKEND_EVENT_NONE;

	if (ctx->dry_run && now_ms - ctx->started_at_ms >= 500U) {
		ctx->status = AUTH_BACKEND_STATUS_RUNNING;
		ctx->phase = EAPOL_SESSION_DONE;
		ctx->authenticated = true;
		return AUTH_BACKEND_EVENT_SUCCESS;
	}

	if (ctx->dry_run)
		return AUTH_BACKEND_EVENT_NONE;

	if (!ctx->reauth_enable && ctx->phase == EAPOL_SESSION_DONE)
		return AUTH_BACKEND_EVENT_NONE;

	for (;;) {
		uint8_t rx[BACKEND_RX_BUF_LEN];
		uint8_t src_mac[RAW_SOCKET_MAC_LEN];
		raw_packet_type_t packet_type = RAW_PACKET_UNKNOWN;
		size_t rx_len = 0;

		if (raw_socket_recv_frame(&ctx->sock, rx, sizeof(rx), &rx_len,
					  src_mac, &packet_type) != 0) {
			LOG_ERROR("%s failed to receive EAPOL frame",
				  ctx->backend_name);
			ctx->status = AUTH_BACKEND_STATUS_FAILED;
			return AUTH_BACKEND_EVENT_FAILURE;
		}

		if (rx_len == 0)
			break;

		if (packet_type == RAW_PACKET_OUTGOING)
			continue;

		event = process_eapol_payload(ctx, rx, rx_len);
		if (event == AUTH_BACKEND_EVENT_SUCCESS ||
		    event == AUTH_BACKEND_EVENT_FAILURE ||
		    event == AUTH_BACKEND_EVENT_TIMEOUT)
			return event;

		packets++;
		if (packets >= 8U)
			break;
	}

	if (now_ms - ctx->last_start_ms >= EAPOL_START_INTERVAL_MS &&
	    ctx->phase == EAPOL_SESSION_WAIT_REQUEST) {
		if (send_start(ctx) != 0) {
			LOG_ERROR("%s failed to retransmit EAPOL-Start",
				  ctx->backend_name);
			ctx->status = AUTH_BACKEND_STATUS_FAILED;
			return AUTH_BACKEND_EVENT_FAILURE;
		}
		return AUTH_BACKEND_EVENT_PROGRESS;
	}

	if (now_ms - ctx->last_wait_log_ms >= 10000U) {
		LOG_DEBUG("%s waiting for authentication packets, phase=%s",
			  ctx->backend_name, phase_to_string(ctx->phase));
		ctx->last_wait_log_ms = now_ms;
	}
	return AUTH_BACKEND_EVENT_NONE;
}

static void eapol_stop(auth_backend_t *backend)
{
	backend_eapol_ctx_t *ctx = backend->ctx;
	uint8_t buf[EAPOL_HEADER_LEN];
	size_t len = 0;

	if (ctx == NULL)
		return;

	if (ctx->logoff_on_stop && !ctx->dry_run && ctx->socket_open &&
	    eapol_build_logoff(ctx->eapol_version, buf, sizeof(buf), &len) == 0)
		(void)raw_socket_send_eapol(&ctx->sock, buf, len);

	if (ctx->socket_open) {
		raw_socket_close(&ctx->sock);
		ctx->socket_open = false;
	}

	if (ctx->status == AUTH_BACKEND_STATUS_RUNNING)
		ctx->status = AUTH_BACKEND_STATUS_STOPPED;
}

static auth_backend_status_t eapol_get_status(auth_backend_t *backend)
{
	backend_eapol_ctx_t *ctx = backend->ctx;

	if (ctx == NULL)
		return AUTH_BACKEND_STATUS_IDLE;
	if (ctx->authenticated && ctx->status == AUTH_BACKEND_STATUS_RUNNING)
		return AUTH_BACKEND_STATUS_SUCCESS;
	return ctx->status;
}

static void eapol_destroy(auth_backend_t *backend)
{
	backend_eapol_ctx_t *ctx;

	if (backend == NULL)
		return;

	ctx = backend->ctx;
	if (ctx != NULL) {
		if (ctx->socket_open)
			raw_socket_close(&ctx->sock);
		memset(ctx->password, 0, sizeof(ctx->password));
		free(ctx);
	}
	backend->ctx = NULL;
}

int backend_eapol_register_named(auth_backend_t *backend, const char *name)
{
	memset(backend, 0, sizeof(*backend));
	backend->name = name != NULL ? name : "standard_eapol";
	backend->init = eapol_init;
	backend->start = eapol_start;
	backend->handle_packet = eapol_handle_packet;
	backend->tick = eapol_tick;
	backend->stop = eapol_stop;
	backend->get_status = eapol_get_status;
	backend->destroy = eapol_destroy;
	return 0;
}

int backend_eapol_register(auth_backend_t *backend)
{
	return backend_eapol_register_named(backend, "standard_eapol");
}
