#ifndef ROBOT_PLC_CRONTORL_ROBOT_PLC_NODE_H
#define ROBOT_PLC_CRONTORL_ROBOT_PLC_NODE_H

#include <atomic>
#include <mutex>
#include <string>

#include <memory>

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>
#include <std_srvs/srv/trigger.hpp>

#include "plc_module/modbus_slave_server.h"
#include "robot_plc_crontorl/robot_plc_controller.h"

namespace robot_plc_crontorl {

class RobotPlcNode : public rclcpp::Node {
 public:
  explicit RobotPlcNode(const rclcpp::NodeOptions& options = rclcpp::NodeOptions());
  ~RobotPlcNode() override;

 private:
  void loadConfig();
  void startServer();
  void stopServer();
  void handleCommandWrite(uint16_t command_value);
  void dispatchPendingTriggerRequest();
  void handleTriggerResult(bool success);
  void syncServerRegistersLocked();

  std::unique_ptr<RobotPlcController> controller_;
  std::unique_ptr<ModbusSlaveServer> server_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr subscription_;
  rclcpp::Client<std_srvs::srv::Trigger>::SharedPtr trigger_client_;
  std::mutex state_mutex_;

  std::string mode_{"tcp"};
  std::string ip_{"0.0.0.0"};
  int port_{1502};
  std::string rtu_device_{"/dev/ttyUSB0"};
  int rtu_baud_{115200};
  char rtu_parity_{'N'};
  int rtu_data_bit_{8};
  int rtu_stop_bit_{1};
  int rtu_slave_id_{1};
  std::string topic_name_{"robot_pose_json"};
  std::string trigger_service_name_{"robot_pose_start"};
  ByteOrder byte_order_{ByteOrder::BigEndian};
};

}  // namespace robot_plc_crontorl

#endif
