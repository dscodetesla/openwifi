#!/bin/sh
set -eu

TARGET_DIR=$1

# Clean files installed by the retired experimental A/B updater and old
# bitstream-link init script when reusing an existing Buildroot output tree.
rm -f "${TARGET_DIR}/usr/sbin/openwifi-firmware-updated" \
	"${TARGET_DIR}/etc/init.d/S45openwifi-firmware-updated" \
	"${TARGET_DIR}/etc/openwifi-update.conf" \
	"${TARGET_DIR}/etc/init.d/S41openwifi-slot-files"

chmod 0755 "${TARGET_DIR}/etc/init.d/S42plutosky-r2-usb-gadget" \
	"${TARGET_DIR}/etc/init.d/S43plutosky-r2-usb-console"


# Preserve the packaged ISC implementation, and install a small boot-profile
# wrapper. Re-running post-build must not save the wrapper as its own backend.
BACKEND="${TARGET_DIR}/usr/libexec/openwifi/dhcp-server"
BOOT_SCRIPT="${TARGET_DIR}/etc/init.d/S80dhcp-server"
if [ -f "${BOOT_SCRIPT}" ]; then
    if ! grep -q '^# openwifi-dhcp-wrapper:' "${BOOT_SCRIPT}"; then
        install -D -m 0755 "${BOOT_SCRIPT}" "${BACKEND}"
    fi
    test -x "${BACKEND}"
    install -m 0755 "$(dirname "$0")/S80openwifi-dhcp-server" "${BOOT_SCRIPT}"
fi
