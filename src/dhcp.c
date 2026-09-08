#define _POSIX_C_SOURCE 200809L

#include "dhcp.h"

#include "log.h"
#include "utils.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define DHCP_RENEW_TIMEOUT_SEC 15U

static int wait_with_timeout(pid_t pid, unsigned int timeout_sec)
{
	unsigned int waited_ms = 0;
	int status;

	for (;;) {
		pid_t got = waitpid(pid, &status, WNOHANG);

		if (got == pid) {
			if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
				return 0;
			return -1;
		}

		if (got < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}

		if (waited_ms >= timeout_sec * 1000U) {
			(void)kill(pid, SIGTERM);
			sleep_msec(500);
			(void)kill(pid, SIGKILL);
			(void)waitpid(pid, &status, 0);
			return -1;
		}

		sleep_msec(100);
		waited_ms += 100U;
	}
}

int dhcp_renew(const char *interface, bool dry_run)
{
	char object[128];
	pid_t pid;

	if (interface == NULL || *interface == '\0')
		return -1;
	if (snprintf(object, sizeof(object), "network.interface.%s", interface) >=
	    (int)sizeof(object))
		return -1;

	if (dry_run) {
		LOG_INFO("dry-run: would renew DHCP on interface %s", interface);
		return 0;
	}

	pid = fork();
	if (pid < 0)
		return -1;
	if (pid == 0) {
		execlp("ubus", "ubus", "call", object, "renew", (char *)NULL);
		_exit(127);
	}

	return wait_with_timeout(pid, DHCP_RENEW_TIMEOUT_SEC);
}

int dhcp_get_wan_ip(const char *interface, char *buf, size_t buf_len)
{
	dhcp_status_t status;

	if (buf == NULL || buf_len == 0)
		return -1;

	if (dhcp_get_status(interface, &status) != 0)
		return -1;

	return safe_strcpy(buf, buf_len, status.ip);
}

static void strip_newline(char *s)
{
	size_t len;

	if (s == NULL)
		return;

	len = strlen(s);
	while (len > 0 && (s[len - 1U] == '\n' || s[len - 1U] == '\r')) {
		s[len - 1U] = '\0';
		len--;
	}
}

static int read_first_line(const char *command, char *buf, size_t buf_len)
{
	FILE *fp;

	if (command == NULL || buf == NULL || buf_len == 0)
		return -1;

	buf[0] = '\0';
	fp = popen(command, "r");
	if (fp == NULL)
		return -1;

	if (fgets(buf, (int)buf_len, fp) == NULL) {
		(void)pclose(fp);
		return -1;
	}

	if (pclose(fp) != 0)
		return -1;

	strip_newline(buf);
	return buf[0] != '\0' ? 0 : -1;
}

static int build_command(char *buf, size_t buf_len,
			 const char *prefix, const char *interface,
			 const char *suffix)
{
	if (strpbrk(interface, "'\\\n\r") != NULL)
		return -1;

	if (snprintf(buf, buf_len, "%s'%s'%s", prefix, interface, suffix) >=
	    (int)buf_len)
		return -1;

	return 0;
}

int dhcp_get_status(const char *interface, dhcp_status_t *status)
{
	char command[256];

	if (interface == NULL || *interface == '\0' || status == NULL)
		return -1;

	memset(status, 0, sizeof(*status));

	if (build_command(command, sizeof(command),
			  "ubus call network.interface.",
			  interface,
			  " status 2>/dev/null | jsonfilter -e '@[\"ipv4-address\"][0].address' 2>/dev/null") == 0 &&
	    read_first_line(command, status->ip, sizeof(status->ip)) == 0)
		status->has_ipv4 = true;

	if (build_command(command, sizeof(command),
			  "ubus call network.interface.",
			  interface,
			  " status 2>/dev/null | jsonfilter -e '@.route[@.target=\"0.0.0.0\"].nexthop' 2>/dev/null") == 0 &&
	    read_first_line(command, status->gateway, sizeof(status->gateway)) == 0)
		status->has_default_route = true;

	if (build_command(command, sizeof(command),
			  "ubus call network.interface.",
			  interface,
			  " status 2>/dev/null | jsonfilter -e '@[\"dns-server\"][0]' 2>/dev/null") == 0)
		(void)read_first_line(command, status->dns, sizeof(status->dns));

	if (!status->has_ipv4 &&
	    build_command(command, sizeof(command),
			  "ip -4 addr show dev ",
			  interface,
			  " 2>/dev/null | awk '/inet / {print $2; exit}'") == 0 &&
	    read_first_line(command, status->ip, sizeof(status->ip)) == 0)
		status->has_ipv4 = true;

	if (!status->has_default_route &&
	    build_command(command, sizeof(command),
			  "ip -4 route show default dev ",
			  interface,
			  " 2>/dev/null | awk '{print $3; exit}'") == 0 &&
	    read_first_line(command, status->gateway, sizeof(status->gateway)) == 0)
		status->has_default_route = true;

	return 0;
}

bool dhcp_is_online(const dhcp_status_t *status)
{
	return status != NULL && status->has_ipv4 && status->has_default_route;
}
