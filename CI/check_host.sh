#!/bin/bash
set -euo pipefail

error() {
    local message=${1}
    echo "error: ${message}"
    exit 1
}

docs="https://sarus.readthedocs.io/en/stable/install/requirements.html#operating-system"
echo "Checking system requirements for Sarus."
echo "See $docs"

# https://apple.stackexchange.com/questions/83939/compare-multi-digit-version-numbers-in-bash/123408#123408
function version {
    echo "$@" | awk -F. '{ printf("%d%03d%03d\n", $1,$2,$3); }';
}

# Linux Kernel
minimum_kernel_version="3.0" # to get containers essentials
obtained_kernel_version=$(cat /proc/version | grep -Eo '[0-9]\.[0-9]+\.[0-9]+' | head -n1)
if [ $(version $obtained_kernel_version) -lt $(version $minimum_kernel_version) ]; then
    error "Linux Kernel should be at least $minimum_kernel_version. Found $obtained_kernel_version"
fi

# kernel modules
grep "\/loop.ko" /lib/modules/`uname -r`/modules.* || error "loop kernel module not loaded"   # Built-in module
grep "\/squashfs.ko" /lib/modules/`uname -r`/modules.* || error "squashfs kernel module not loaded"   # Built-in module
lsmod | grep overlay    || error "overlayfs kernel module not loaded"

# mount-utils
expected_mount_version="2.20.0" # to get autoclear flag automatically be enabled
obtained_mount_version=$(mount --version | grep -Eo '[0-9]\.[0-9]+\.[0-9]+' | head -n1)
if [ $(version $obtained_mount_version) -lt $(version $expected_mount_version) ]; then
    error "mount should come from mount-utils >= $expected_mount_version. Found $obtained_mount_version"
fi

echo "Successfully run $0"
