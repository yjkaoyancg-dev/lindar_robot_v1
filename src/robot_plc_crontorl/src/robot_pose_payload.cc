#include "robot_plc_crontorl/robot_pose_payload.h"

#include <nlohmann/json.hpp>

namespace robot_plc_crontorl {

namespace {

bool readFloatField(const nlohmann::json& object, const char* key, float& out) {
  if (!object.contains(key) || !object.at(key).is_number()) {
    return false;
  }
  out = object.at(key).get<float>();
  return true;
}

}  // namespace

std::optional<RobotPosePayload> parseRobotPosePayload(
    const std::string& json_text) {
  nlohmann::json root;
  try {
    root = nlohmann::json::parse(json_text);
  } catch (...) {
    return std::nullopt;
  }

  if (!root.contains("position") || !root.at("position").is_object() ||
      !root.contains("attitude") || !root.at("attitude").is_object()) {
    return std::nullopt;
  }

  RobotPosePayload payload;
  const auto& position = root.at("position");
  const auto& attitude = root.at("attitude");

  if (!readFloatField(position, "x", payload.x) ||
      !readFloatField(position, "y", payload.y) ||
      !readFloatField(position, "z", payload.z) ||
      !readFloatField(attitude, "roll", payload.roll) ||
      !readFloatField(attitude, "pitch", payload.pitch) ||
      !readFloatField(attitude, "yaw", payload.yaw) ||
      !readFloatField(root, "confidence", payload.confidence)) {
    return std::nullopt;
  }

  return payload;
}

}  // namespace robot_plc_crontorl
