include $(TOPDIR)/rules.mk

PKG_NAME:=sysu-authd
PKG_VERSION:=0.1.2
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/sysu-authd
  SECTION:=net
  CATEGORY:=Network
  TITLE:=SYSU Ruijie/OpenWrt auto authentication daemon
  DEPENDS:=+libc
endef

define Package/sysu-authd/description
  sysu-authd is a layered C daemon for SYSU campus network
  802.1X/Ruijie-compatible auto authentication on OpenWrt WAN interfaces.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) $(CURDIR)/src $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CPPFLAGS) $(TARGET_CFLAGS) \
		-std=c11 -Wall -Wextra -Wpedantic \
		-o $(PKG_BUILD_DIR)/sysu-authd \
		$(PKG_BUILD_DIR)/src/*.c $(TARGET_LDFLAGS)
endef

define Package/sysu-authd/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sysu-authd $(1)/usr/sbin/sysu-authd
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/etc/config/sysu-authd $(1)/etc/config/sysu-authd
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/etc/init.d/sysu-authd $(1)/etc/init.d/sysu-authd
	$(INSTALL_DIR) $(1)/etc/sysu-authd
endef

define Package/sysu-authd/conffiles
/etc/config/sysu-authd
endef

$(eval $(call BuildPackage,sysu-authd))
