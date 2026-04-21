#ifndef ROBOT_PLC_CRONTORL_REGISTER_LAYOUT_H
#define ROBOT_PLC_CRONTORL_REGISTER_LAYOUT_H

#include <cstddef>
#include <cstdint>

namespace robot_plc_crontorl {

enum class RobotPlcState : uint16_t {
  kIdle = 0,
  kWaiting = 1,
  kReady = 2,
};

struct RegisterLayout {
  static constexpr std::size_t kRegisterCount = 16;
  static constexpr std::size_t kCmd = 0;
  static constexpr std::size_t kState = 1;
  static constexpr std::size_t kPosition = 2;
  static constexpr std::size_t kAttitude = 8;
  static constexpr std::size_t kConfidence = 14;
  static constexpr std::size_t kPositionRegisterCount = 6;
  static constexpr std::size_t kAttitudeRegisterCount = 6;
  static constexpr std::size_t kConfidenceRegisterCount = 2;
};

}  // namespace robot_plc_crontorl

#endif
