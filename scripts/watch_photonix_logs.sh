#!/usr/bin/env bash
LOGDIR="$HOME/photonix_logs"
mkdir -p "$LOGDIR"
tail -f \
  "$LOGDIR/lidar.log" \
  "$LOGDIR/range_detector.log" \
  "$LOGDIR/range_bridge.log" \
  "$LOGDIR/plc.log"
