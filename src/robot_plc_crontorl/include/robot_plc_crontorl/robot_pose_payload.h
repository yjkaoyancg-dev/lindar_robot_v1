#ifndef ROBOT_PLC_CRONTORL_ROBOT_POSE_PAYLOAD_H
#define ROBOT_PLC_CRONTORL_ROBOT_POSE_PAYLOAD_H

#include <optional>
#include <string>

namespace robot_plc_crontorl {

struct RobotPosePayload {
  float x{0.0F};
  float y{0.0F};
  float z{0.0F};
  float roll{0.0F};
  float pitch{0.0F};
  float yaw{0.0F};
  float confidence{0.0F};
};

std::optional<RobotPosePayload> parseRobotPosePayload(
    const std::string& json_text);

}  // namespace robot_plc_crontorl

#endif
