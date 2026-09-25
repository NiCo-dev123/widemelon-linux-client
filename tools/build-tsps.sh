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

compiler="${TRIMUI_TSPS_SDK}/host/bin/aarch64-none-linux-gnu-g++"
if [[ ! -x "${compiler}" ]]; then
    echo "Trimui compiler not found: ${compiler}" >&2
    exit 1
fi

cmake -S "${project_dir}" -B "${project_dir}/${BUILD_DIR}" -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build "${project_dir}/${BUILD_DIR}" --parallel
