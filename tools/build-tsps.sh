#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
config_file="${project_dir}/tools/build-tsps.conf"

if [[ ! -f "${config_file}" ]]; then
    echo "Missing ${config_file}" >&2
    echo "Copy tools/build-tsps.conf.example to tools/build-tsps.conf and set TRIMUI_TSPS_SDK." >&2
    exit 1
fi

# shellcheck source=/dev/null
source "${config_file}"

: "${TRIMUI_TSPS_SDK:?TRIMUI_TSPS_SDK must be set in tools/build-tsps.conf}"
export TRIMUI_TSPS_SDK
: "${BUILD_DIR:=build-tsps}"
: "${DIST_DIR:=dist}"

compiler="${TRIMUI_TSPS_SDK}/host/bin/aarch64-none-linux-gnu-g++"
if [[ ! -x "${compiler}" ]]; then
    echo "Trimui compiler not found: ${compiler}" >&2
    exit 1
fi

cmake -S "${project_dir}" -B "${project_dir}/${BUILD_DIR}" -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build "${project_dir}/${BUILD_DIR}" --parallel

app_dir="${project_dir}/${DIST_DIR}/WideMelonClient"
lib_dir="${app_dir}/lib"
font_dir="${app_dir}/assets/fonts"
sysroot_lib="${TRIMUI_TSPS_SDK}/host/aarch64-buildroot-linux-gnu/sysroot/usr/lib"

if [[ "${app_dir}" == "${project_dir}" || "${app_dir}" == "/" ]]; then
    echo "Refusing unsafe application output path: ${app_dir}" >&2
    exit 1
fi
rm -rf "${app_dir}"
mkdir -p "${lib_dir}" "${font_dir}"
cp "${project_dir}/${BUILD_DIR}/widemelon-client" "${app_dir}/widemelon-client"
cp "${project_dir}/packaging/spruce/WideMelonClient/config.json" "${project_dir}/packaging/spruce/WideMelonClient/launch.sh" "${project_dir}/packaging/spruce/WideMelonClient/wmclient.png" "${app_dir}/"
cp "${project_dir}/assets/fonts/Roboto-Regular.ttf" "${font_dir}/Roboto-Regular.ttf"
cp -R "${project_dir}/assets/backgrounds" "${project_dir}/assets/icons" "${app_dir}/assets/"
cp "${project_dir}/widemelon-client-ui.conf" "${app_dir}/widemelon-client-ui.conf"
cp "${project_dir}/config/widemelon-client.conf.example" "${app_dir}/widemelon-client.conf"
cp -L "${sysroot_lib}/libSDL2-2.0.so.0" "${sysroot_lib}/libSDL2_ttf-2.0.so.0" "${sysroot_lib}/libSDL2_image-2.0.so.0" "${sysroot_lib}/libjpeg.so.8" "${lib_dir}/"
chmod 755 "${app_dir}/widemelon-client" "${app_dir}/launch.sh"

version="$(tr -d "\r\n" < "${project_dir}/VERSION")"
archive="${project_dir}/${DIST_DIR}/WideMelonClient-${version}.zip"
(
    cd "${project_dir}/${DIST_DIR}"
    zip -rq -FS "${archive}" WideMelonClient
)

echo "Ready to copy: ${app_dir}"
echo "Release archive: ${archive}"
