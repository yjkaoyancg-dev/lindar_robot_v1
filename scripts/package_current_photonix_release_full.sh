#!/usr/bin/env bash
set -eo pipefail

# Enhanced full snapshot packager for current Photonix machine state.
# Usage:
#   chmod +x package_current_photonix_release_full.sh
#   ./package_current_photonix_release_full.sh [OUTPUT_PARENT_DIR]
#
# Default output parent dir: ~/Desktop

OUT_PARENT="${1:-$HOME/Desktop}"
STAMP="$(date +%F_%H%M%S)"
PKG_NAME="photonix_actual_full_snapshot_${STAMP}"
OUT_DIR="${OUT_PARENT%/}/${PKG_NAME}"
SNAP_DIR="$OUT_DIR/snapshot"
ENV_DIR="$OUT_DIR/environment"
SYS_DIR="$OUT_DIR/system"
DOC_DIR="$OUT_DIR/docs"
LOG_DIR="$OUT_DIR/logs"
SRC_DIR="$OUT_DIR/source"
CONF_DIR="$OUT_DIR/configs"
TPL_DIR="$OUT_DIR/templates"
SCRIPT_DIR="$OUT_DIR/scripts"
MISC_DIR="$OUT_DIR/misc"

mkdir -p "$OUT_DIR" "$SNAP_DIR" "$ENV_DIR" "$SYS_DIR" "$DOC_DIR" "$LOG_DIR" "$SRC_DIR" "$CONF_DIR" "$TPL_DIR" "$SCRIPT_DIR" "$MISC_DIR"

copy_if_exists() {
  local src="$1"
  local dst="$2"
  if [ -e "$src" ]; then
    mkdir -p "$(dirname "$dst")"
    cp -a "$src" "$dst"
    return 0
  fi
  return 1
}

copy_dir_if_exists() {
  local src="$1"
  local dst="$2"
  if [ -d "$src" ]; then
    mkdir -p "$(dirname "$dst")"
    cp -a "$src" "$dst"
    return 0
  fi
  return 1
}

run_capture() {
  local outfile="$1"; shift
  {
    echo "# CMD: $*"
    echo
    "$@"
  } > "$outfile" 2>&1 || true
}

run_capture_shell() {
  local outfile="$1"; shift
  {
    echo "# CMD: $*"
    echo
    bash -lc "$*"
  } > "$outfile" 2>&1 || true
}

# -----------------------------
# 1) Source / workspace snapshot
# -----------------------------
if [ -d "$HOME/work/Photonix" ]; then
  echo "[1/9] Copying workspace: $HOME/work/Photonix"
  copy_dir_if_exists "$HOME/work/Photonix" "$SRC_DIR/Photonix"
fi

# Common older backup directories from this debugging session, if present
for d in \
  "$HOME/nodes_old_"* \
  "$HOME/service_old_"* \
  "$HOME/env_old_"*; do
  [ -e "$d" ] || continue
  base="$(basename "$d")"
  copy_if_exists "$d" "$MISC_DIR/$base"
done

# User-provided original archives / installers often useful for reconstruction
for f in \
  "$HOME/Photonix.zip" \
  "$HOME/service.zip" \
  "$HOME/ZWKJ_ubuntu3.1_amd64.deb" \
  "$HOME/zwkjTools-2.0.0-Linux.deb" \
  "$HOME/photonix-suite_"*.deb \
  "$HOME/astronyx-suite-"*.deb; do
  [ -e "$f" ] || continue
  copy_if_exists "$f" "$MISC_DIR/$(basename "$f")"
done

# -----------------------------
# 2) Runtime configs/templates
# -----------------------------
echo "[2/9] Copying runtime configs and templates"
copy_dir_if_exists "/opt/zwkj/configs" "$CONF_DIR/opt_zwkj_configs"
copy_dir_if_exists "/opt/zwkj/templates" "$TPL_DIR/opt_zwkj_templates"

# Specific known configs (copied individually as well for convenience)
for f in \
  "/opt/zwkj/configs/lidar_config.json" \
  "/opt/zwkj/configs/detect_config.json" \
  "/opt/zwkj/configs/range_density_detect_config.json" \
  "/opt/zwkj/configs/robot_plc_crontorl.json"; do
  [ -e "$f" ] || continue
  copy_if_exists "$f" "$CONF_DIR/$(basename "$f")"
done

# -----------------------------
# 3) User scripts / docs / notes
# -----------------------------
echo "[3/9] Copying user scripts and docs"
for f in \
  "$HOME/start_photonix_stack.sh" \
  "$HOME/start_stack_modular.sh" \
  "$HOME/start_stack_cluster.sh" \
  "$HOME/start_stack_range_density.sh" \
  "$HOME/stop_photonix_stack.sh" \
  "$HOME/watch_photonix_logs.sh" \
  "$HOME/trigger_plc_test.sh" \
  "$HOME/trigger_plc_test_wait.sh" \
  "$HOME/fake_plc_master_flow.sh" \
  "$HOME/check_photonix_stack.sh" \
  "$HOME/package_current_photonix_release.sh" \
  "$HOME/package_current_photonix_release_full.sh"; do
  [ -e "$f" ] || continue
  copy_if_exists "$f" "$SCRIPT_DIR/$(basename "$f")"
done

# Common docs in home/workspace that may matter
for f in \
  "$HOME/history.txt" \
  "$HOME/README.md" \
  "$HOME/README_FIELD_RELEASE.md"; do
  [ -e "$f" ] || continue
  copy_if_exists "$f" "$DOC_DIR/$(basename "$f")"
done

# Copy all markdown/txt docs found in likely locations (top-level only + docs folders)
for d in "$HOME/work/Photonix" "$HOME/work/Photonix/docs" "$HOME"; do
  [ -d "$d" ] || continue
  find "$d" -maxdepth 2 -type f \( -name '*.md' -o -name '*.txt' \) 2>/dev/null | while read -r f; do
    base="${f#$HOME/}"
    mkdir -p "$DOC_DIR/$(dirname "$base")"
    cp -a "$f" "$DOC_DIR/$base"
  done
done

# -----------------------------
# 4) Systemd services / deployment files
# -----------------------------
echo "[4/9] Capturing service files and deployment-related files"
mkdir -p "$SYS_DIR/systemd"
for svc in \
  "/etc/systemd/system/zwkjLidar.service" \
  "/etc/systemd/system/zwkjDetect.service" \
  "/etc/systemd/system/zwkjPlc.service" \
  "/etc/systemd/system/snapd.service.d/disable-refresh.conf"; do
  [ -e "$svc" ] || continue
  cp -a "$svc" "$SYS_DIR/systemd/$(basename "$svc")" 2>/dev/null || true
done

# If service unit has related drop-ins, capture them
for d in \
  "/etc/systemd/system/zwkjLidar.service.d" \
  "/etc/systemd/system/zwkjDetect.service.d" \
  "/etc/systemd/system/zwkjPlc.service.d"; do
  [ -d "$d" ] || continue
  cp -a "$d" "$SYS_DIR/systemd/" 2>/dev/null || true
done

# -----------------------------
# 5) Logs / runtime evidence
# -----------------------------
echo "[5/9] Copying logs"
copy_dir_if_exists "$HOME/photonix_logs" "$LOG_DIR/photonix_logs"
[ -d "/var/log" ] && mkdir -p "$LOG_DIR/system_snippets"
run_capture_shell "$LOG_DIR/system_snippets/journal_zwkjLidar.txt" "journalctl -u zwkjLidar -n 200 --no-pager"
run_capture_shell "$LOG_DIR/system_snippets/journal_zwkjDetect.txt" "journalctl -u zwkjDetect -n 200 --no-pager"
run_capture_shell "$LOG_DIR/system_snippets/journal_zwkjPlc.txt" "journalctl -u zwkjPlc -n 200 --no-pager"

# -----------------------------
# 6) Environment and package snapshots
# -----------------------------
echo "[6/9] Capturing environment snapshots"
run_capture "$ENV_DIR/uname_a.txt" uname -a
run_capture "$ENV_DIR/date.txt" date
run_capture "$ENV_DIR/env.txt" env
run_capture "$ENV_DIR/python_version.txt" python3 --version
run_capture_shell "$ENV_DIR/ros_distro.txt" 'echo "ROS_DISTRO=${ROS_DISTRO:-}"'
run_capture_shell "$ENV_DIR/ros2_pkg_list.txt" 'source /opt/ros/iron/setup.bash && ros2 pkg list | sort'
run_capture_shell "$ENV_DIR/colcon_list.txt" 'cd "$HOME/work/Photonix" 2>/dev/null && source /opt/ros/iron/setup.bash && colcon list'
run_capture_shell "$ENV_DIR/dpkg_ros.txt" "dpkg -l | egrep 'ros-|libmodbus|mbpoll|pcl|nlohmann|colcon|python3-colcon'"
run_capture_shell "$ENV_DIR/pip_freeze.txt" 'python3 -m pip freeze'
run_capture_shell "$ENV_DIR/apt_manual.txt" 'apt-mark showmanual | sort'

# Build and workspace metadata
run_capture_shell "$ENV_DIR/workspace_tree_top.txt" 'find "$HOME/work/Photonix" -maxdepth 3 -type f | sort | sed "s|$HOME||" | head -n 500'
run_capture_shell "$ENV_DIR/source_file_inventory.txt" 'find "$HOME/work/Photonix" -type f | sort | sed "s|$HOME||"'

# -----------------------------
# 7) Device / port / hardware snapshots
# -----------------------------
echo "[7/9] Capturing USB/serial snapshots"
run_capture "$SYS_DIR/lsusb.txt" lsusb
run_capture_shell "$SYS_DIR/dev_serial_by_id.txt" 'ls -l /dev/serial/by-id/'
run_capture_shell "$SYS_DIR/dev_ttyUSB_ACM.txt" 'ls -l /dev/ttyUSB* /dev/ttyACM* 2>/dev/null'
run_capture_shell "$SYS_DIR/lsof_serial.txt" 'lsof /dev/serial/by-id/* 2>/dev/null || true'
run_capture_shell "$SYS_DIR/groups.txt" 'groups'
run_capture_shell "$SYS_DIR/ip_addr.txt" 'ip addr'
run_capture_shell "$SYS_DIR/ip_route.txt" 'ip route'
run_capture_shell "$SYS_DIR/ss_lntp.txt" 'ss -lntp'
run_capture_shell "$SYS_DIR/systemctl_zwkj.txt" 'systemctl status zwkjLidar --no-pager -l; echo; systemctl status zwkjDetect --no-pager -l; echo; systemctl status zwkjPlc --no-pager -l'
run_capture_shell "$SYS_DIR/dmesg_tail.txt" 'sudo dmesg | tail -n 200'

# -----------------------------
# 8) Git snapshots if repo exists
# -----------------------------
echo "[8/9] Capturing git state (if available)"
if [ -d "$HOME/work/Photonix/.git" ]; then
  run_capture_shell "$SNAP_DIR/git_status.txt" 'cd "$HOME/work/Photonix" && git status --short --branch'
  run_capture_shell "$SNAP_DIR/git_log.txt" 'cd "$HOME/work/Photonix" && git log --oneline --decorate -n 50'
  run_capture_shell "$SNAP_DIR/git_diff.txt" 'cd "$HOME/work/Photonix" && git diff'
else
  echo "No .git directory found in ~/work/Photonix" > "$SNAP_DIR/git_status.txt"
fi

# -----------------------------
# 9) Release notes / inventory / readme
# -----------------------------
echo "[9/9] Writing inventory and README"
cat > "$OUT_DIR/README_FIRST.txt" <<TXT
Photonix current-machine full snapshot

This package is generated FROM THE CURRENT MACHINE STATE.
It is intended to preserve:
- current workspace source
- current runtime configs
- current templates
- current scripts
- service files (if accessible)
- logs and environment snapshots
- USB/serial mapping snapshots

Important:
1. This is the most complete snapshot we can produce from the current machine.
2. It is not a guarantee that another computer can run without installing dependencies.
3. Check environment/ and system/ to reproduce dependencies and USB role mapping.
4. Check source/Photonix and configs/ first when restoring.
5. Current verified RTU role mapping should be checked from docs / snapshots, not guessed.
TXT

# Summaries
find "$OUT_DIR" -type f | sort > "$OUT_DIR/FILE_LIST.txt"

cat > "$OUT_DIR/RELEASE_INFO.txt" <<TXT
Generated: $(date)
Hostname : $(hostname)
User     : $(whoami)

Workspace source:
- $HOME/work/Photonix

Runtime config snapshot:
- /opt/zwkj/configs
- /opt/zwkj/templates

Known important serial devices at packaging time:
$(ls -l /dev/serial/by-id/ 2>/dev/null || true)

Known important notes:
- This snapshot intentionally captures CURRENT machine state.
- If previously forgotten source files exist under ~/work/Photonix, they are included.
- If files were only edited elsewhere and never saved into the current machine filesystem, no packager can recover them.
TXT

# Archive
cd "$OUT_PARENT"
tar -czf "${PKG_NAME}.tar.gz" "$PKG_NAME"
zip -qr "${PKG_NAME}.zip" "$PKG_NAME"

echo

echo "Done. Output directory:" 
echo "  $OUT_DIR"
echo "Archives:" 
echo "  ${OUT_PARENT%/}/${PKG_NAME}.tar.gz"
echo "  ${OUT_PARENT%/}/${PKG_NAME}.zip"
