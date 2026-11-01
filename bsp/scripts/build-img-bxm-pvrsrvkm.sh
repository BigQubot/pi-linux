#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
pvr_top="$(cd "${script_dir}/../modules/gpu/img-bxm/linux/rogue_km" && pwd)"

: "${KERNELDIR:?KERNELDIR must point to a prepared kernel build tree}"

kernel_cross_compile="${KERNEL_CROSS_COMPILE:-${CROSS_COMPILE:-aarch64-linux-gnu-}}"
kernel_arch="${KERNEL_ARCH:-${ARCH:-arm64}}"
gpu_build_type="${GPU_BUILD_TYPE:-release}"
pvr_out="${PVR_OUT:-${pvr_top}/binary_armbian_sunxi_linux_nulldrmws_${gpu_build_type}}"

kernel_cc="${KERNEL_CC:-${kernel_cross_compile}gcc}"
kernel_ld="${KERNEL_LD:-${kernel_cross_compile}ld}"
kernel_nm="${KERNEL_NM:-${kernel_cross_compile}gcc-nm}"
kernel_objcopy="${KERNEL_OBJCOPY:-${kernel_cross_compile}objcopy}"

make -C "${pvr_top}" \
	PVR_BUILD_DIR=sunxi_linux \
	OUT="${pvr_out}" \
	ARCH="${kernel_arch}" \
	KERNELDIR="${KERNELDIR}" \
	KERNEL_CROSS_COMPILE="${kernel_cross_compile}" \
	CROSS_COMPILE="${kernel_cross_compile}" \
	KERNEL_CC="${kernel_cc}" \
	KERNEL_LD="${kernel_ld}" \
	KERNEL_NM="${kernel_nm}" \
	KERNEL_OBJCOPY="${kernel_objcopy}" \
	GPU_BUILD_TYPE="${gpu_build_type}" \
	kbuild

module="${pvr_out}/target_aarch64/pvrsrvkm.ko"
if [[ ! -f "${module}" ]]; then
	echo "pvrsrvkm.ko was not produced at ${module}" >&2
	exit 1
fi

if [[ -n "${INSTALL_MOD_PATH:-}" ]]; then
	kernel_release="${KERNEL_VERSION:-}"
	if [[ -z "${kernel_release}" ]]; then
		kernel_release="$(make -s -C "${KERNELDIR}" ARCH="${kernel_arch}" CROSS_COMPILE="${kernel_cross_compile}" kernelrelease)"
	fi

	install_dir="${INSTALL_MOD_PATH}/lib/modules/${kernel_release}/extra"
	install -d "${install_dir}"
	install -m 0644 "${module}" "${install_dir}/pvrsrvkm.ko"

	if command -v depmod >/dev/null 2>&1; then
		depmod -b "${INSTALL_MOD_PATH}" "${kernel_release}" || true
	fi
fi

printf '%s\n' "${module}"
