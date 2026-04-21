<!--
 * @FilePath: /Photonix/src/plc_module/test/pub_test.md
 * @Author: mwt 911608720@qq.com
 * @Date: 2025-04-27 12:54:22
 * @Description: 
 * @Copyright: 2025 by mwt, All Rights Reserved.
-->
ros2 topic pub /t4/plc_task std_msgs/msg/String "data: '{\"id\":\"audioAlarm_zw_d0603217_ca6d_11f0_9ff6_56e78058c55a\", \"address\":16, \"type\":\"int16\", \"source\":\"holding\", \"value\":1, \"seq\":\"demo-seq-001\"}'"

ros2 topic pub /t4/plc_task std_msgs/msg/String "data: '{\"id\":\"audioAlarm_zw_d0603217_ca6d_11f0_9ff6_56e78058c55a\", \"address\":16, \"type\":\"int16\", \"source\":\"holding\", \"value\":0, \"seq\":\"demo-seq-001\"}'"

nohup ros2 topic pub --once /u_e1ccbc26eab6/plc_task std_msgs/msg/String \
  "data: '{\"id\":\"clearance_05d58a79_557b_11f0_891e_001b21be568a\", \"address\":26, \"type\":\"int16\", \"source\":\"holding\", \"value\":78, \"seq\":\"demo-seq-001\"}'" \
  > /dev/null 2>&1 &



ros2 topic pub /t4/plc_task std_msgs/msg/String "data: '{\"id\":\"audioAlarm_zw_d0603217_ca6d_11f0_9ff6_56e78058c55a\", \"address\":0, \"type\":\"int16\", \"source\":\"holding\", \"value\":0, \"seq\":\"demo-seq-001\"}'"