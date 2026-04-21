#include <rclcpp/rclcpp.hpp>

#include "robot_plc_crontorl/robot_plc_node.h"

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<robot_plc_crontorl::RobotPlcNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
