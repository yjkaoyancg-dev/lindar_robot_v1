#!/usr/bin/env bash
set -eo pipefail

DEV="/dev/serial/by-id/usb-1a86_USB_Single_Serial_5ACC004110-if00"
SID=1

cd "$HOME/work/Photonix"
source /opt/ros/iron/setup.bash
source install/setup.bash

echo "等待第一帧点云..."
ros2 topic echo /pointcloud_BFZ9IEC9 --once > /dev/null
echo "点云已到，开始主站流程"

echo "写 cmd=0"
mbpoll -1 -m rtu -a "$SID" -b 115200 -P none -t 4 -r 1 "$DEV" 0
sleep 1

echo "写 cmd=1"
mbpoll -1 -m rtu -a "$SID" -b 115200 -P none -t 4 -r 1 "$DEV" 1
sleep 1
for i in {1..20}; do
  echo "第 $i 次轮询..."
  OUT=$(mbpoll -1 -m rtu -a "$SID" -b 115200 -P none -t 4 -r 1 -c 16 "$DEV")
  echo "$OUT"

  STATE=$(echo "$OUT" | awk '/^\[2\]:/ {print $2}')
  if [ "$STATE" = "2" ]; then
    echo "识别完成"
    exit 0
  fi
  sleep 1
done

echo "超时：15~20 秒内没有到 state=2"
exit 1
