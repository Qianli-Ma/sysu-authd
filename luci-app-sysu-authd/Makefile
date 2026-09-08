include $(TOPDIR)/rules.mk

LUCI_TITLE:=LuCI support for SYSU Auth
LUCI_DESCRIPTION:=Web configuration, status, and service controls for sysu-authd.
LUCI_DEPENDS:=+sysu-authd +rpcd +luci-base
LUCI_PKGARCH:=all

PKG_VERSION:=0.1.0
PKG_RELEASE:=1
PKG_LICENSE:=MIT
PKG_LICENSE_FILES:=LICENSE

include $(TOPDIR)/feeds/luci/luci.mk

# call BuildPackage - OpenWrt buildroot signature
