#!/usr/bin/env bash
set -e

MASTER_DEV="/dev/serial/by-id/usb-1a86_USB_Single_Serial_5ACC004110-if00"

echo "=== Step 1: 写 cmd = 0 ==="
mbpoll -1 -m rtu -a 1 -b 115200 -P none -t 4 -r 1 "$MASTER_DEV" 0

sleep 1

echo
echo "=== Step 2: 写 cmd = 1 ==="
mbpoll -1 -m rtu -a 1 -b 115200 -P none -t 4 -r 1 "$MASTER_DEV" 1

sleep 1

echo
echo "=== Step 3: 读取 12 个 holding registers ==="
mbpoll -1 -m rtu -a 1 -b 115200 -P none -t 4 -r 1 -c 12 "$MASTER_DEV"
