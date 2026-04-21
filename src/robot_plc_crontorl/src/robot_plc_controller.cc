#include "robot_plc_crontorl/robot_plc_controller.h"

namespace robot_plc_crontorl {

RobotPlcController::RobotPlcController(ByteOrder byte_order)
    : PlcDevice(byte_order),
      registers_(RegisterLayout::kRegisterCount, 0U) {
  registers_[RegisterLayout::kState] =
      static_cast<uint16_t>(RobotPlcState::kIdle);
}

void RobotPlcController::connect() {}

void RobotPlcController::disconnect() {}

void RobotPlcController::sendRequest() {}

void RobotPlcController::handleResponse() {}

std::string RobotPlcController::getDeviceInfo() const {
  return "RobotPlcController";
}

void RobotPlcController::handleCommandRegister(uint16_t value) {
  registers_[RegisterLayout::kCmd] = value;
  const bool rising_edge = (last_cmd_ == 0U && value == 1U);
  const bool master_reset =
      (last_cmd_ == 1U && value == 0U && state() == RobotPlcState::kReady);
  last_cmd_ = value;

  if (master_reset) {
    clearDataRegisters();
    registers_[RegisterLayout::kState] =
        static_cast<uint16_t>(RobotPlcState::kIdle);
    trigger_ready_ = false;
    pending_trigger_requests_ = 0;
    return;
  }

  if (!rising_edge) {
    return;
  }

  clearDataRegisters();
  registers_[RegisterLayout::kState] =
      static_cast<uint16_t>(RobotPlcState::kWaiting);
  trigger_ready_ = false;
  pending_trigger_requests_ = 1;
}

void RobotPlcController::handleTriggerResponse(bool success) {
  pending_trigger_requests_ = 0;
  trigger_ready_ = success;
}

bool RobotPlcController::applyTopicPayload(const std::string& json_text) {
  if (state() != RobotPlcState::kWaiting || !trigger_ready_) {
    return false;
  }

  const auto payload = parseRobotPosePayload(json_text);
  if (!payload.has_value()) {
    return false;
  }

  writeFloat(RegisterLayout::kPosition, payload->x);
  writeFloat(RegisterLayout::kPosition + 2, payload->y);
  writeFloat(RegisterLayout::kPosition + 4, payload->z);
  writeFloat(RegisterLayout::kAttitude, payload->roll);
  writeFloat(RegisterLayout::kAttitude + 2, payload->pitch);
  writeFloat(RegisterLayout::kAttitude + 4, payload->yaw);
  writeFloat(RegisterLayout::kConfidence, payload->confidence);
  registers_[RegisterLayout::kState] =
      static_cast<uint16_t>(RobotPlcState::kReady);
  return true;
}

RobotPlcState RobotPlcController::state() const {
  return static_cast<RobotPlcState>(registers_[RegisterLayout::kState]);
}

const std::vector<uint16_t>& RobotPlcController::registers() const {
  return registers_;
}

int RobotPlcController::pendingTriggerRequests() const {
  return pending_trigger_requests_;
}

bool RobotPlcController::consumePendingTriggerRequest() {
  if (pending_trigger_requests_ <= 0) {
    return false;
  }
  --pending_trigger_requests_;
  return true;
}

void RobotPlcController::clearDataRegisters() {
  for (std::size_t index = RegisterLayout::kPosition;
       index < RegisterLayout::kRegisterCount; ++index) {
    registers_[index] = 0U;
  }
}

void RobotPlcController::writeFloat(std::size_t start_index, float value) {
  const auto encoded = encodeValue(value);
  for (std::size_t offset = 0; offset < encoded.size(); ++offset) {
    registers_[start_index + offset] = encoded[offset];
  }
}

}  // namespace robot_plc_crontorl
