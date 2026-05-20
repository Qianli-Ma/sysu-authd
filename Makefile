CC ?= cc
AR ?= ar

TARGET := sysu-authd
BUILD_DIR := build
SRC_DIR := src

SRCS := \
	$(SRC_DIR)/main.c \
	$(SRC_DIR)/daemon.c \
	$(SRC_DIR)/state_machine.c \
	$(SRC_DIR)/config.c \
	$(SRC_DIR)/netif.c \
	$(SRC_DIR)/raw_socket.c \
	$(SRC_DIR)/eapol.c \
	$(SRC_DIR)/eap.c \
	$(SRC_DIR)/auth_backend.c \
	$(SRC_DIR)/backend_eapol.c \
	$(SRC_DIR)/backend_ruijie.c \
	$(SRC_DIR)/profile.c \
	$(SRC_DIR)/dhcp.c \
	$(SRC_DIR)/watchdog.c \
	$(SRC_DIR)/status.c \
	$(SRC_DIR)/md5.c \
	$(SRC_DIR)/log.c \
	$(SRC_DIR)/utils.c

OBJS := $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

CPPFLAGS ?=
CFLAGS ?= -O2 -g
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -Werror -MMD -MP
LDFLAGS ?=
LDLIBS ?=

.PHONY: all clean install uninstall check

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

install: $(TARGET)
	install -d $(DESTDIR)/usr/sbin
	install -m 0755 $(TARGET) $(DESTDIR)/usr/sbin/$(TARGET)

uninstall:
	rm -f $(DESTDIR)/usr/sbin/$(TARGET)

check: all
	printf 'secret\n' >/tmp/sysu-authd-password
	chmod 0600 /tmp/sysu-authd-password
	./$(TARGET) --config docs/example.conf --dry-run

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

-include $(DEPS)
