#define _POSIX_C_SOURCE 200809L

#include "config.h"
#include "daemon.h"
#include "log.h"
#include "profile.h"
#include "utils.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static daemon_ctx_t *g_ctx;

static void usage(FILE *out)
{
	fprintf(out,
		"Usage: sysu-authd [options]\n"
		"\n"
		"Options:\n"
		"  -c, --config PATH      Load key=value config file\n"
		"  -f, --foreground       Log to stderr instead of syslog\n"
		"  -n, --dry-run          Run state machine without raw socket/DHCP effects\n"
		"  -h, --help             Show this help\n");
}

static void handle_signal(int signo)
{
	(void)signo;
	if (g_ctx != NULL)
		daemon_request_stop(g_ctx);
}

static int install_signal_handlers(void)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handle_signal;
	sigemptyset(&sa.sa_mask);

	if (sigaction(SIGTERM, &sa, NULL) != 0)
		return -1;
	if (sigaction(SIGINT, &sa, NULL) != 0)
		return -1;
	return 0;
}

int main(int argc, char **argv)
{
	const char *config_path = NULL;
	bool foreground = false;
	bool dry_run = false;
	config_t cfg;
	profile_t profile;
	daemon_ctx_t ctx;
	int rc;
	int i;

	config_set_defaults(&cfg);

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
			if (i + 1 >= argc) {
				usage(stderr);
				return EXIT_FAILURE;
			}
			config_path = argv[++i];
		} else if (strcmp(argv[i], "-f") == 0 ||
			   strcmp(argv[i], "--foreground") == 0) {
			foreground = true;
		} else if (strcmp(argv[i], "-n") == 0 ||
			   strcmp(argv[i], "--dry-run") == 0) {
			dry_run = true;
			foreground = true;
		} else if (strcmp(argv[i], "-h") == 0 ||
			   strcmp(argv[i], "--help") == 0) {
			usage(stdout);
			return EXIT_SUCCESS;
		} else {
			fprintf(stderr, "unknown option: %s\n", argv[i]);
			usage(stderr);
			return EXIT_FAILURE;
		}
	}

	log_init(cfg.log_level, true);
	if (config_load_file(&cfg, config_path) != 0)
		return EXIT_FAILURE;

	if (profile_load(&profile, cfg.profile) != 0) {
		fprintf(stderr, "unknown profile: %s\n", cfg.profile);
		return EXIT_FAILURE;
	}
	profile_apply_to_config(&profile, &cfg);
	if (cfg.eapol_version >= 1U && cfg.eapol_version <= 2U)
		profile.eapol_version = (uint8_t)cfg.eapol_version;
	if (cfg.identity_format[0] != '\0')
		(void)safe_strcpy(profile.identity_format,
				  sizeof(profile.identity_format),
				  cfg.identity_format);

	log_init(cfg.log_level, foreground);

	if (install_signal_handlers() != 0) {
		LOG_ERROR("failed to install signal handlers");
		return EXIT_FAILURE;
	}

	if (daemon_init(&ctx, &cfg, &profile, dry_run, foreground) != 0)
		return EXIT_FAILURE;

	g_ctx = &ctx;
	rc = daemon_run(&ctx);
	g_ctx = NULL;
	daemon_cleanup(&ctx);

	return rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
