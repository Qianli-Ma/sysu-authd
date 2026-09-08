#!/bin/sh
set -eu

SDK="${SDK:-}"
WITH_LUCI="${WITH_LUCI:-1}"
FORCE="${FORCE:-0}"
PROJECT_DIR="$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)"

case "$WITH_LUCI" in
	0|1) ;;
	*) echo "WITH_LUCI must be 0 or 1" >&2; exit 1 ;;
esac

case "$FORCE" in
	0|1) ;;
	*) echo "FORCE must be 0 or 1" >&2; exit 1 ;;
esac

if [ -z "$SDK" ]; then
	echo "usage: SDK=/path/to/openwrt-sdk [WITH_LUCI=0] [FORCE=1] $0" >&2
	exit 1
fi

SDK="$(CDPATH='' cd -- "$SDK" && pwd)"
if [ ! -f "$SDK/rules.mk" ] || [ ! -d "$SDK/package" ]; then
	echo "not an OpenWrt SDK/build root: $SDK" >&2
	exit 1
fi
if [ "$WITH_LUCI" = "1" ] && [ ! -f "$SDK/feeds/luci/luci.mk" ]; then
	echo "LuCI feed is unavailable: $SDK/feeds/luci/luci.mk" >&2
	echo "install/update the LuCI feed, or set WITH_LUCI=0" >&2
	exit 1
fi

"$PROJECT_DIR/scripts/check-openwrt-assets.sh"

CORE_DEST="$SDK/package/sysu-authd"
LUCI_DEST="$SDK/package/luci-app-sysu-authd"

for dest in "$CORE_DEST" "$LUCI_DEST"; do
	[ "$dest" = "$LUCI_DEST" ] && [ "$WITH_LUCI" != "1" ] && continue
	if [ -e "$dest" ]; then
		if [ "$FORCE" != "1" ]; then
			echo "destination already exists: $dest (set FORCE=1 to replace it)" >&2
			exit 1
		fi
		rm -rf "$dest"
	fi
done

mkdir -p "$CORE_DEST"
cp "$PROJECT_DIR/openwrt/Makefile" "$CORE_DEST/Makefile"
cp -R "$PROJECT_DIR/openwrt/files" "$CORE_DEST/files"
cp -R "$PROJECT_DIR/src" "$CORE_DEST/src"

if [ "$WITH_LUCI" = "1" ]; then
	mkdir -p "$LUCI_DEST"
	cp "$PROJECT_DIR/luci-app-sysu-authd/Makefile" "$LUCI_DEST/Makefile"
	cp "$PROJECT_DIR/luci-app-sysu-authd/LICENSE" "$LUCI_DEST/LICENSE"
	cp -R "$PROJECT_DIR/luci-app-sysu-authd/htdocs" "$LUCI_DEST/htdocs"
	cp -R "$PROJECT_DIR/luci-app-sysu-authd/root" "$LUCI_DEST/root"
	if [ -d "$PROJECT_DIR/luci-app-sysu-authd/po" ]; then
		cp -R "$PROJECT_DIR/luci-app-sysu-authd/po" "$LUCI_DEST/po"
	fi
fi

echo "staged sysu-authd in $CORE_DEST"
[ "$WITH_LUCI" = "1" ] && echo "staged luci-app-sysu-authd in $LUCI_DEST"
