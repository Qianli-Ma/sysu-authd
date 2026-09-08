#!/bin/sh
set -eu

SDK="${SDK:-}"
PROJECT_DIR="$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)"
OUT_DIR="${OUT_DIR:-$PROJECT_DIR/dist/openwrt}"
PKG_NAME="${PKG_NAME:-sysu-authd}"
PKG_VERSION="${PKG_VERSION:-0.1.0}"
PKG_RELEASE="${PKG_RELEASE:-1}"
LUCI_PKG_NAME="${LUCI_PKG_NAME:-luci-app-sysu-authd}"
LUCI_PKG_ARCH="all"
BUILD_LUCI="${BUILD_LUCI:-1}"

usage() {
	cat <<EOF_USAGE
用法：
  $0 /path/to/openwrt-sdk
  SDK=/path/to/openwrt-sdk $0

默认生成 sysu-authd 和 luci-app-sysu-authd 软件包。
只构建核心软件包：BUILD_LUCI=0 $0 /path/to/openwrt-sdk
EOF_USAGE
}

case "$#" in
	0) ;;
	1)
		case "$1" in
			-h|--help) usage; exit 0 ;;
		esac
		if [ -n "$SDK" ] && [ "$SDK" != "$1" ]; then
			echo "SDK 环境变量与命令行参数不一致" >&2
			exit 1
		fi
		SDK="$1"
		;;
	*) usage >&2; exit 1 ;;
esac

case "$BUILD_LUCI" in
	0|1) ;;
	*) echo "BUILD_LUCI 必须是 0 或 1" >&2; exit 1 ;;
esac

if [ -z "$SDK" ]; then
	usage >&2
	exit 1
fi
if [ ! -d "$SDK/staging_dir" ]; then
	echo "不是有效的 OpenWrt SDK（缺少 staging_dir）：$SDK" >&2
	exit 1
fi

"$PROJECT_DIR/scripts/check-openwrt-assets.sh"

SDK="$(CDPATH='' cd -- "$SDK" && pwd)"
TOOLCHAIN="${TOOLCHAIN:-$(find "$SDK/staging_dir" -maxdepth 1 -type d -name 'toolchain-*' | head -n 1)}"
TARGET_STAGING="${TARGET_STAGING:-$(find "$SDK/staging_dir" -maxdepth 1 -type d -name 'target-*' | head -n 1)}"

if [ -z "$TOOLCHAIN" ] || [ -z "$TARGET_STAGING" ]; then
	echo "无法在 $SDK/staging_dir 下找到工具链或目标暂存目录" >&2
	exit 1
fi

if [ -f "$TOOLCHAIN/info.mk" ]; then
	# shellcheck disable=SC1091
	. "$TOOLCHAIN/info.mk"
fi

TARGET_CROSS="${TARGET_CROSS:-}"
if [ -z "$TARGET_CROSS" ]; then
	TARGET_CROSS="$(find "$TOOLCHAIN/bin" -maxdepth 1 \( -type f -o -type l \) -name '*-gcc' | sed 's/-gcc$//' | head -n 1)"
	TARGET_CROSS="${TARGET_CROSS##*/}-"
fi

CC="${CC:-${TARGET_CROSS}gcc}"
AR="${AR:-${TARGET_CROSS}ar}"
STRIP="${STRIP:-${TARGET_CROSS}strip}"
PKG_ARCH="${PKG_ARCH:-$(basename "$TARGET_STAGING" | sed -e 's/^target-//' -e 's/_musl.*$//' -e 's/+/_/g')}"

if [ ! -x "$TOOLCHAIN/bin/$CC" ]; then
	echo "找不到 OpenWrt 交叉编译器：$TOOLCHAIN/bin/$CC" >&2
	exit 1
fi

mkdir -p "$OUT_DIR"
tar_time="${SOURCE_DATE_EPOCH_TIME:-2023-11-15 06:13:20 UTC}"

build_ipk() {
	root="$1"
	work="$2"
	ipk="$3"

	cat > "$work/debian-binary" <<'EOF_DEBIAN'
2.0
EOF_DEBIAN

	( cd "$root" && tar --format=gnu --sort=name --owner=0 --group=0 \
		--numeric-owner --mtime="$tar_time" -cf - . | gzip -n > "$work/data.tar.gz" )
	( cd "$work/control" && tar --format=gnu --sort=name --owner=0 --group=0 \
		--numeric-owner --mtime="$tar_time" -cf - . | gzip -n > "$work/control.tar.gz" )

	rm -f "$ipk"
	( cd "$work" && tar --format=gnu --sort=name --owner=0 --group=0 \
		--numeric-owner --mtime="$tar_time" -cf - ./debian-binary ./data.tar.gz ./control.tar.gz | gzip -n > "$ipk" )
}

(
	cd "$PROJECT_DIR"
	make clean
	PATH="$TOOLCHAIN/bin:$PATH" STAGING_DIR="$SDK/staging_dir" make \
		CC="$CC" \
		AR="$AR" \
		CFLAGS="${CFLAGS:--Os -pipe}" \
		CPPFLAGS="${CPPFLAGS:--I$TARGET_STAGING/usr/include -I$TARGET_STAGING/include}" \
		LDFLAGS="${LDFLAGS:--L$TARGET_STAGING/usr/lib -L$TARGET_STAGING/lib -Wl,--gc-sections -static-libgcc}" \
		TARGET="$PKG_NAME-openwrt"
	cp "$PKG_NAME-openwrt" "$OUT_DIR/$PKG_NAME"
	PATH="$TOOLCHAIN/bin:$PATH" STAGING_DIR="$SDK/staging_dir" "$STRIP" "$OUT_DIR/$PKG_NAME"
)

ROOT="$OUT_DIR/pkgroot"
WORK="$OUT_DIR/ipkbuild"
LUCI_ROOT="$OUT_DIR/luci-pkgroot"
LUCI_WORK="$OUT_DIR/luci-ipkbuild"
IPK="$OUT_DIR/${PKG_NAME}_${PKG_VERSION}-${PKG_RELEASE}_${PKG_ARCH}.ipk"
LUCI_IPK="$OUT_DIR/${LUCI_PKG_NAME}_${PKG_VERSION}-${PKG_RELEASE}_${LUCI_PKG_ARCH}.ipk"

rm -rf "$ROOT" "$WORK" "$LUCI_ROOT" "$LUCI_WORK"
mkdir -p "$ROOT/usr/sbin" "$ROOT/etc/config" "$ROOT/etc/init.d" \
	"$ROOT/etc/sysu-authd" "$WORK/control"

install -m 0755 "$OUT_DIR/$PKG_NAME" "$ROOT/usr/sbin/$PKG_NAME"
install -m 0644 "$PROJECT_DIR/openwrt/files/etc/config/sysu-authd" \
	"$ROOT/etc/config/sysu-authd"
install -m 0755 "$PROJECT_DIR/openwrt/files/etc/init.d/sysu-authd" \
	"$ROOT/etc/init.d/sysu-authd"
DAEMON_SIZE="$(du -sk "$ROOT" | awk '{print $1}')"

cat > "$WORK/control/control" <<EOF_CONTROL
Package: $PKG_NAME
Version: $PKG_VERSION-$PKG_RELEASE
Depends: libc
Source: local
SourceName: $PKG_NAME
Section: net
Architecture: $PKG_ARCH
Installed-Size: $DAEMON_SIZE
Maintainer: sysu-authd project
Description: SYSU Ruijie/OpenWrt auto authentication daemon
EOF_CONTROL

cat > "$WORK/control/conffiles" <<'EOF_CONFFILES'
/etc/config/sysu-authd
EOF_CONFFILES

build_ipk "$ROOT" "$WORK" "$IPK"

if [ "$BUILD_LUCI" = "1" ]; then
	mkdir -p "$LUCI_ROOT/www" "$LUCI_WORK/control"
	cp -R "$PROJECT_DIR/luci-app-sysu-authd/htdocs/." "$LUCI_ROOT/www/"
	cp -R "$PROJECT_DIR/luci-app-sysu-authd/root/." "$LUCI_ROOT/"
	LUCI_SIZE="$(du -sk "$LUCI_ROOT" | awk '{print $1}')"

	cat > "$LUCI_WORK/control/control" <<EOF_CONTROL
Package: $LUCI_PKG_NAME
Version: $PKG_VERSION-$PKG_RELEASE
Depends: sysu-authd, rpcd, luci-base
Source: local
SourceName: $LUCI_PKG_NAME
Section: luci
Architecture: $LUCI_PKG_ARCH
Installed-Size: $LUCI_SIZE
Maintainer: sysu-authd project
Description: LuCI web interface for sysu-authd
EOF_CONTROL

	cat > "$LUCI_WORK/control/postinst" <<'EOF_POSTINST'
#!/bin/sh
[ -n "${IPKG_INSTROOT}" ] || {
	rm -f /tmp/luci-indexcache
	rm -rf /tmp/luci-modulecache/
	killall -HUP rpcd 2>/dev/null || true
}
exit 0
EOF_POSTINST
	chmod 0755 "$LUCI_WORK/control/postinst"
	build_ipk "$LUCI_ROOT" "$LUCI_WORK" "$LUCI_IPK"
fi

file "$OUT_DIR/$PKG_NAME"
file "$IPK"
if [ "$BUILD_LUCI" = "1" ]; then
	file "$LUCI_IPK"
	sha256sum "$OUT_DIR/$PKG_NAME" "$IPK" "$LUCI_IPK"
else
	sha256sum "$OUT_DIR/$PKG_NAME" "$IPK"
fi

echo
echo "构建完成，产物位于：$OUT_DIR"
