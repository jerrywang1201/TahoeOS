#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
ELF_FILE="${BUILD_DIR}/IAP_F411.elf"
OPENOCD_CFG="${ROOT_DIR}/openocd.cfg"

usage() {
  cat <<'EOF'
Usage: tools/flash_debug.sh [flash|gdb|all]

  flash  - program ELF via OpenOCD and exit
  gdb    - start OpenOCD and attach GDB (no auto-flash)
  all    - start OpenOCD, flash, then enter GDB
EOF
}

if [[ $# -ne 1 ]]; then
  usage
  exit 1
fi

if [[ ! -f "${ELF_FILE}" ]]; then
  echo "ELF not found: ${ELF_FILE}"
  echo "Build first: cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake && cmake --build build --parallel"
  exit 1
fi

openocd_start() {
  openocd -f "${OPENOCD_CFG}" >"${BUILD_DIR}/openocd.log" 2>&1 &
  OPENOCD_PID=$!
  for _ in {1..40}; do
    if nc -z 127.0.0.1 3333 >/dev/null 2>&1; then
      return 0
    fi
    sleep 0.1
  done
  echo "OpenOCD failed to start. Check ${BUILD_DIR}/openocd.log"
  exit 1
}

openocd_stop() {
  if [[ -n "${OPENOCD_PID:-}" ]]; then
    kill "${OPENOCD_PID}" >/dev/null 2>&1 || true
  fi
}

case "$1" in
  flash)
    openocd -f "${OPENOCD_CFG}" -c "program ${ELF_FILE} verify reset exit"
    ;;
  gdb)
    openocd_start
    trap openocd_stop EXIT
    arm-none-eabi-gdb -q "${ELF_FILE}" \
      -ex "target extended-remote :3333" \
      -ex "monitor reset halt"
    ;;
  all)
    openocd_start
    trap openocd_stop EXIT
    arm-none-eabi-gdb -q "${ELF_FILE}" \
      -ex "target extended-remote :3333" \
      -ex "monitor reset halt" \
      -ex "load" \
      -ex "monitor reset"
    ;;
  *)
    usage
    exit 1
    ;;
esac
