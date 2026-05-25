# Operator GUI PR1

Industrial observation GUI for the existing lidar robot deployment.

PR1 is intentionally read-only:

- Does not start or stop ROS2 nodes.
- Does not modify JSON configuration files.
- Does not switch detector modes.
- Does not connect to serial ports.
- Does not write PLC registers.
- Does not modify systemd services or deploy scripts.

## Run

From the repository root on the Ubuntu IPC:

```bash
source /opt/ros/iron/setup.bash
source install/setup.bash
python3 -m operator_gui.operator_gui.main
```

Or:

```bash
./operator_gui/scripts/start_operator_gui.sh
```

## Runtime Paths

PR1 reads current deployment files in this order:

Configuration:

1. `~/deploy_v1/runtime/opt_zwkj/configs`
2. `/opt/zwkj/configs`
3. repository fallback `configs/runtime`

Logs:

1. `~/deploy_v1/runtime/opt_zwkj/logs`
2. `/opt/zwkj/logs`
3. `$HOME/photonix_logs`

## Dependencies

Install GUI dependencies in the Python environment used by ROS2:

```bash
pip install -r operator_gui/requirements.txt
```

`rclpy` is expected to come from the ROS2 installation.

## Package

```bash
./operator_gui/scripts/package_operator_gui_pr1.sh
```

This creates `operator_gui_pr1.zip` in the repository root.

For PR2 device scan:

```bash
./operator_gui/scripts/package_operator_gui_pr2.sh
```

This creates `operator_gui_pr2_device_scan.zip` in the repository root.

For PR4 visualization:

```bash
./operator_gui/scripts/package_operator_gui_pr4.sh
```

This creates `operator_gui_pr4_visualization.zip` in the repository root.

For PR5 export:

```bash
./operator_gui/scripts/package_operator_gui_pr5.sh
```

This creates `operator_gui_pr5_export.zip` in the repository root.

## Install On Another IPC

Copy `operator_gui_pr1.zip` to the target Ubuntu IPC, then run:

```bash
bash install_operator_gui_pr1.sh operator_gui_pr1.zip
```

or from an unpacked repository:

```bash
./operator_gui/scripts/install_operator_gui_pr1.sh operator_gui_pr1.zip
```

For PR2:

```bash
./operator_gui/scripts/install_operator_gui_pr2.sh operator_gui_pr2_device_scan.zip
```

For PR4:

```bash
./operator_gui/scripts/install_operator_gui_pr4.sh operator_gui_pr4_visualization.zip
```

For PR5:

```bash
./operator_gui/scripts/install_operator_gui_pr5.sh operator_gui_pr5_export.zip
```

PR5 export files are written to:

```text
~/deploy_v1/operator_gui_exports/
```

## PR6 Output Gate

PLC output writes are gated by `output_enabled` in `robot_plc_crontorl.json`.

Default:

```json
"output_enabled": false
```

When false, the PLC node can still run and serve Modbus registers, but incoming
detection results are not written into position, attitude, or confidence output
registers. The GUI only displays this state; it does not toggle the gate.

For PR6 packaging:

```bash
./operator_gui/scripts/package_operator_gui_pr6.sh
```

This creates `operator_gui_pr6_output_gate.zip` in the repository root.

For PR6 install:

```bash
./operator_gui/scripts/install_operator_gui_pr6.sh operator_gui_pr6_output_gate.zip
```
