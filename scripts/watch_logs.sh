#!/usr/bin/env bash
LOGDIR="$HOME/photonix_logs"
mkdir -p "$LOGDIR"
tail -f "$LOGDIR"/*.log
