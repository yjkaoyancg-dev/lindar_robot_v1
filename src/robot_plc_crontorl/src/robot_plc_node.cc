#include "robot_plc_crontorl/robot_plc_node.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>

#include "plc_module/modbus_rtu_slave.h"
#include "plc_module/modbus_tcp_slave.h"

using namespace std::chrono_literals;

namespace robot_plc_crontorl {

namespace {

nlohmann::json read_json_file(const std::string& path) {
  std::ifstream stream(path);
  if (!stream.is_open()) {
    throw std::runtime_error("failed to open config file: " + path);
  }

  nlohmann::json config;
  stream >> config;
  return config;
}

}  // namespace

RobotPlcNode::RobotPlcNode(const rclcpp::NodeOptions& options)
    : Node("robot_plc_crontorl", options) {
  loadConfig();
  controller_ = std::make_unique<RobotPlcController>(byte_order_);
  controller_->setOutputEnabled(output_enabled_);

  if (output_enabled_) {
    RCLCPP_WARN(get_logger(),
                "PLC output gate ENABLED: detection results may be written to output registers.");
  } else {
    RCLCPP_WARN(get_logger(),
                "PLC output gate DISABLED: dry-run mode, detection results will not be written to output registers.");
  }

  subscription_ = create_subscription<std_msgs::msg::String>(
      topic_name_, rclcpp::QoS(10),
      [this](const std_msgs::msg::String::SharedPtr msg) {
        std::lock_guard<std::mutex> lk(state_mutex_);
        if (controller_->applyTopicPayload(msg->data)) {
          syncServerRegistersLocked();
          RCLCPP_INFO(
              get_logger(),
              "Detection result accepted and written to PLC output registers.");
        } else if (!controller_->outputEnabled()) {
          syncServerRegistersLocked();
          RCLCPP_WARN(
              get_logger(),
              "PLC output gate is DISABLED (dry-run); detection result ignored and output registers remain zero.");
        } else {
          RCLCPP_WARN(
              get_logger(),
              "Detection result ignored because current PLC state is not ready for a new payload.");
        }
      });

  trigger_client_ = create_client<std_srvs::srv::Trigger>(trigger_service_name_);
  RCLCPP_INFO(get_logger(),
              "robot_plc_crontorl node started, result_topic='%s', trigger_service='%s'",
              topic_name_.c_str(), trigger_service_name_.c_str());
  startServer();
}

RobotPlcNode::~RobotPlcNode() { stopServer(); }

void RobotPlcNode::loadConfig() {
  const std::string default_config =
      "/opt/zwkj/configs/robot_plc_crontorl.json";
  const auto config_path =
      declare_parameter<std::string>("config_path", default_config);

  if (!std::filesystem::exists(config_path)) {
    RCLCPP_WARN(get_logger(), "config file not found, using defaults: %s",
                config_path.c_str());
    return;
  }

  try {
    const auto config = read_json_file(config_path);

    if (config.contains("mode") && config.at("mode").is_string()) {
      mode_ = config.at("mode").get<std::string>();
    }
    if (config.contains("byte_order") && config.at("byte_order").is_string()) {
      const auto order = config.at("byte_order").get<std::string>();
      byte_order_ =
          (order == "little") ? ByteOrder::LittleEndian : ByteOrder::BigEndian;
    }
    if (config.contains("output_enabled") &&
        config.at("output_enabled").is_boolean()) {
      output_enabled_ = config.at("output_enabled").get<bool>();
    }
    if (config.contains("topic_name") && config.at("topic_name").is_string()) {
      topic_name_ = config.at("topic_name").get<std::string>();
    }
    if (config.contains("trigger_service_name") &&
        config.at("trigger_service_name").is_string()) {
      trigger_service_name_ =
          config.at("trigger_service_name").get<std::string>();
    }

    if (config.contains("tcp") && config.at("tcp").is_object()) {
      const auto& tcp = config.at("tcp");
      if (tcp.contains("ip") && tcp.at("ip").is_string()) {
        ip_ = tcp.at("ip").get<std::string>();
      }
      if (tcp.contains("port") && tcp.at("port").is_number_integer()) {
        port_ = tcp.at("port").get<int>();
      }
    } else {
      if (config.contains("ip") && config.at("ip").is_string()) {
        ip_ = config.at("ip").get<std::string>();
      }
      if (config.contains("port") && config.at("port").is_number_integer()) {
        port_ = config.at("port").get<int>();
      }
    }

    if (config.contains("rtu") && config.at("rtu").is_object()) {
      const auto& rtu = config.at("rtu");
      if (rtu.contains("device") && rtu.at("device").is_string()) {
        rtu_device_ = rtu.at("device").get<std::string>();
      }
      if (rtu.contains("baud") && rtu.at("baud").is_number_integer()) {
        rtu_baud_ = rtu.at("baud").get<int>();
      }
      if (rtu.contains("parity") && rtu.at("parity").is_string() &&
          !rtu.at("parity").get<std::string>().empty()) {
        rtu_parity_ = rtu.at("parity").get<std::string>().front();
      }
      if (rtu.contains("data_bit") && rtu.at("data_bit").is_number_integer()) {
        rtu_data_bit_ = rtu.at("data_bit").get<int>();
      }
      if (rtu.contains("stop_bit") && rtu.at("stop_bit").is_number_integer()) {
        rtu_stop_bit_ = rtu.at("stop_bit").get<int>();
      }
      if (rtu.contains("slave_id") && rtu.at("slave_id").is_number_integer()) {
        rtu_slave_id_ = rtu.at("slave_id").get<int>();
      }
    }
  } catch (const std::exception& ex) {
    RCLCPP_WARN(get_logger(), "failed to load config file '%s': %s",
                config_path.c_str(), ex.what());
    return;
  }

  RCLCPP_INFO(
      get_logger(),
      "config loaded: mode=%s, byte_order=%s, topic=%s, trigger_service=%s",
      mode_.c_str(), byte_order_ == ByteOrder::LittleEndian ? "little" : "big",
      topic_name_.c_str(), trigger_service_name_.c_str());
  RCLCPP_WARN(get_logger(), "PLC output_enabled=%s (%s)",
              output_enabled_ ? "true" : "false",
              output_enabled_ ? "output writes allowed"
                              : "dry-run, output writes blocked");
}

void RobotPlcNode::startServer() {
  if (mode_ == "rtu") {
    server_ = std::make_unique<ModbusRTUSlave>(
        rtu_device_, rtu_baud_, rtu_parity_, rtu_data_bit_, rtu_stop_bit_,
        rtu_slave_id_, static_cast<int>(RegisterLayout::kRegisterCount),
        static_cast<int>(RegisterLayout::kCmd));
  } else {
    server_ = std::make_unique<ModbusTCPSlave>(
        ip_, port_, static_cast<int>(RegisterLayout::kRegisterCount),
        static_cast<int>(RegisterLayout::kCmd));
  }

  server_->setCommandWriteHandler(
      [this](uint16_t command_value) { handleCommandWrite(command_value); });

  {
    std::lock_guard<std::mutex> lk(state_mutex_);
    syncServerRegistersLocked();
  }
  server_->start();

  if (mode_ == "rtu") {
    RCLCPP_INFO(get_logger(),
                "Modbus RTU slave started: device=%s baud=%d parity=%c data=%d stop=%d slave_id=%d",
                rtu_device_.c_str(), rtu_baud_, rtu_parity_, rtu_data_bit_,
                rtu_stop_bit_, rtu_slave_id_);
  } else {
    RCLCPP_INFO(get_logger(), "Modbus TCP slave started: %s:%d",
                ip_.c_str(), port_);
  }
}

void RobotPlcNode::stopServer() {
  if (server_) {
    RCLCPP_INFO(get_logger(), "Stopping Modbus slave server");
    server_->stop();
    server_.reset();
  }
}

void RobotPlcNode::handleCommandWrite(uint16_t command_value) {
  bool should_dispatch = false;
  RobotPlcState current_state = RobotPlcState::kIdle;
  {
    std::lock_guard<std::mutex> lk(state_mutex_);
    controller_->handleCommandRegister(command_value);
    syncServerRegistersLocked();
    should_dispatch = controller_->consumePendingTriggerRequest();
    current_state = controller_->state();
  }

  RCLCPP_INFO(get_logger(), "command register written: cmd=%u state=%u",
              command_value, static_cast<unsigned int>(current_state));

  if (should_dispatch) {
    RCLCPP_INFO(get_logger(),
                "start command rising edge detected; dispatching trigger request");
    dispatchPendingTriggerRequest();
  } else if (command_value == 0U && current_state == RobotPlcState::kIdle) {
    RCLCPP_INFO(get_logger(),
                "master reset received; state is idle and data registers are zero");
  }
}

void RobotPlcNode::dispatchPendingTriggerRequest() {
  if (!trigger_client_->wait_for_service(200ms)) {
    RCLCPP_WARN(get_logger(), "trigger service '%s' is unavailable",
                trigger_service_name_.c_str());
    handleTriggerResult(false);
    return;
  }

  RCLCPP_INFO(get_logger(), "calling detector Trigger service");
  auto request = std::make_shared<std_srvs::srv::Trigger::Request>();
  using SharedFuture = rclcpp::Client<std_srvs::srv::Trigger>::SharedFuture;
  trigger_client_->async_send_request(
      request, [this](SharedFuture future) {
        bool success = false;
        try {
          const auto response = future.get();
          success = response && response->success;
        } catch (...) {
          success = false;
        }
        handleTriggerResult(success);
      });
}

void RobotPlcNode::handleTriggerResult(bool success) {
  {
    std::lock_guard<std::mutex> lk(state_mutex_);
    controller_->handleTriggerResponse(success);
    syncServerRegistersLocked();
  }

  if (success) {
    RCLCPP_INFO(get_logger(),
                "detector accepted trigger; waiting for result topic payload");
  } else {
    RCLCPP_WARN(get_logger(), "detector trigger failed; state remains waiting");
  }
}

void RobotPlcNode::syncServerRegistersLocked() {
  if (server_ && controller_) {
    server_->syncHoldingRegisters(controller_->registers());
  }
}

}  // namespace robot_plc_crontorl
