#ifndef ROBOT_PLC_CRONTORL_ROBOT_PLC_CONTROLLER_H
#define ROBOT_PLC_CRONTORL_ROBOT_PLC_CONTROLLER_H

#include <cstdint>
#include <string>
#include <vector>

#include "plc_module/plc_device_base.h"
#include "robot_plc_crontorl/register_layout.h"
#include "robot_plc_crontorl/robot_pose_payload.h"

namespace robot_plc_crontorl {

class RobotPlcController : private PlcDevice {
 public:
  explicit RobotPlcController(ByteOrder byte_order = ByteOrder::BigEndian);
  void connect() override;
  void disconnect() override;
  void sendRequest() override;
  void handleResponse() override;
  std::string getDeviceInfo() const override;

  void handleCommandRegister(uint16_t value);
  void handleTriggerResponse(bool success);
  bool applyTopicPayload(const std::string& json_text);
  void setOutputEnabled(bool enabled);
  bool outputEnabled() const;

  RobotPlcState state() const;
  const std::vector<uint16_t>& registers() const;
  int pendingTriggerRequests() const;
  bool consumePendingTriggerRequest();

 private:
  void clearDataRegisters();
  void writeFloat(std::size_t start_index, float value);

  std::vector<uint16_t> registers_;
  uint16_t last_cmd_{0};
  bool trigger_ready_{false};
  bool output_enabled_{false};
  int pending_trigger_requests_{0};
};

}  // namespace robot_plc_crontorl

#endif
