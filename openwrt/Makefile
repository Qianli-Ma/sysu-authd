include $(TOPDIR)/rules.mk

PKG_NAME:=sysu-authd
PKG_VERSION:=0.1.0
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
	$(CP) $(TOPDIR)/../sysu-authd/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) \
		CC="$(TARGET_CC)" \
		AR="$(TARGET_AR)" \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)"
endef

define Package/sysu-authd/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sysu-authd $(1)/usr/sbin/sysu-authd
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/etc/config/sysu-authd $(1)/etc/config/sysu-authd
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/etc/init.d/sysu-authd $(1)/etc/init.d/sysu-authd
	$(INSTALL_DIR) $(1)/usr/libexec/rpcd
	$(INSTALL_BIN) ./files/usr/libexec/rpcd/sysu-authd $(1)/usr/libexec/rpcd/sysu-authd
	$(INSTALL_DIR) $(1)/etc/sysu-authd
endef

define Package/sysu-authd/postinst
#!/bin/sh
[ -n "$${IPKG_INSTROOT}" ] || killall -HUP rpcd 2>/dev/null || true
exit 0
endef

define Package/luci-app-sysu-authd
  SECTION:=luci
  CATEGORY:=LuCI
  SUBMENU:=3. Applications
  TITLE:=LuCI support for sysu-authd
  DEPENDS:=+sysu-authd +rpcd +luci-base
  PKGARCH:=all
endef

define Package/luci-app-sysu-authd/description
  LuCI web interface for sysu-authd status, service actions, and UCI settings.
endef

define Package/luci-app-sysu-authd/install
	$(INSTALL_DIR) $(1)/www
	$(CP) ./luci-app-sysu-authd/htdocs/* $(1)/www/
	$(INSTALL_DIR) $(1)/
	$(CP) ./luci-app-sysu-authd/root/* $(1)/
endef

define Package/luci-app-sysu-authd/postinst
#!/bin/sh
[ -n "$${IPKG_INSTROOT}" ] || {
	rm -f /tmp/luci-indexcache
	rm -rf /tmp/luci-modulecache/
	killall -HUP rpcd 2>/dev/null || true
}
exit 0
endef

$(eval $(call BuildPackage,sysu-authd))
$(eval $(call BuildPackage,luci-app-sysu-authd))
