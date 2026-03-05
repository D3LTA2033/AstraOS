#!/bin/bash
# =============================================================================
# AstraOS - QEMU Launch Script
# =============================================================================
# Usage: ./scripts/run-qemu.sh [--debug]
#
# Options:
#   --debug    Start QEMU with GDB server on port 1234 (paused at start)

set -e

ISO="build/astraos.iso"

if [ ! -f "$ISO" ]; then
    echo "Error: $ISO not found. Run 'make iso' first."
    exit 1
fi

QEMU_ARGS=(
    -cdrom "$ISO"
    -m 128M
    -serial stdio
    -no-reboot
    -no-shutdown
)

if [ "$1" = "--debug" ]; then
    echo "[DEBUG] Starting QEMU with GDB server on :1234 (paused)"
    QEMU_ARGS+=(-s -S)
fi

echo "[QEMU] Booting AstraOS..."
exec qemu-system-i386 "${QEMU_ARGS[@]}"
