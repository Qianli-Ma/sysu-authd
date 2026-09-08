#!/bin/sh
set -eu

PROJECT_DIR="$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)"

required_files='openwrt/files/etc/config/sysu-authd
openwrt/files/etc/init.d/sysu-authd
luci-app-sysu-authd/Makefile
luci-app-sysu-authd/LICENSE
luci-app-sysu-authd/root/usr/libexec/rpcd/sysu-authd
luci-app-sysu-authd/root/usr/share/luci/menu.d/luci-app-sysu-authd.json
luci-app-sysu-authd/root/usr/share/rpcd/acl.d/luci-app-sysu-authd.json
luci-app-sysu-authd/htdocs/luci-static/resources/view/sysu-authd/overview.js
luci-app-sysu-authd/po/templates/sysu-authd.pot
luci-app-sysu-authd/po/zh_Hans/sysu-authd.po'

missing=0
for path in $required_files; do
	if [ ! -f "$PROJECT_DIR/$path" ]; then
		echo "missing OpenWrt packaging asset: $path" >&2
		missing=1
	fi
done

[ "$missing" -eq 0 ]
echo "OpenWrt packaging assets are complete"
