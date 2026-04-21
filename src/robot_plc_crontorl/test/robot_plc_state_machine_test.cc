#include <gtest/gtest.h>

#include "robot_plc_crontorl/robot_plc_controller.h"

namespace robot_plc_crontorl {
namespace {

TEST(RobotPlcStateMachineTest, startsIdleWithZeroedDataRegisters) {
  RobotPlcController controller;

  EXPECT_EQ(controller.state(), RobotPlcState::kIdle);
  for (std::size_t index = RegisterLayout::kPosition;
       index < RegisterLayout::kRegisterCount; ++index) {
    EXPECT_EQ(controller.registers()[index], 0U);
  }
}

TEST(RobotPlcStateMachineTest, commandRisingEdgeClearsDataAndRequestsTrigger) {
  RobotPlcController controller;

  controller.handleCommandRegister(1U);

  EXPECT_EQ(controller.state(), RobotPlcState::kWaiting);
  EXPECT_EQ(controller.pendingTriggerRequests(), 1);
  for (std::size_t index = RegisterLayout::kPosition;
       index < RegisterLayout::kRegisterCount; ++index) {
    EXPECT_EQ(controller.registers()[index], 0U);
  }
}

TEST(RobotPlcStateMachineTest, validPayloadTransitionsToReadyAfterTriggerSuccess) {
  RobotPlcController controller;

  controller.handleCommandRegister(1U);
  controller.handleTriggerResponse(true);

  ASSERT_TRUE(controller.applyTopicPayload(
      R"({"position":{"x":1.0,"y":2.0,"z":3.0},"attitude":{"roll":4.0,"pitch":5.0,"yaw":6.0},"confidence":0.42})"));
  EXPECT_EQ(controller.state(), RobotPlcState::kReady);
  EXPECT_TRUE(controller.registers()[RegisterLayout::kConfidence] != 0U ||
              controller.registers()[RegisterLayout::kConfidence + 1] != 0U);
}

TEST(RobotPlcStateMachineTest, invalidPayloadDoesNotLeaveWaiting) {
  RobotPlcController controller;

  controller.handleCommandRegister(1U);
  controller.handleTriggerResponse(true);

  EXPECT_FALSE(controller.applyTopicPayload(R"({"position":{}})"));
  EXPECT_EQ(controller.state(), RobotPlcState::kWaiting);
}

TEST(RobotPlcStateMachineTest,
     readyToIdleResetClearsRegistersWhenMasterWritesZero) {
  RobotPlcController controller;

  controller.handleCommandRegister(1U);
  controller.handleTriggerResponse(true);
  ASSERT_TRUE(controller.applyTopicPayload(
      R"({"position":{"x":1.0,"y":2.0,"z":3.0},"attitude":{"roll":4.0,"pitch":5.0,"yaw":6.0},"confidence":0.42})"));
  ASSERT_EQ(controller.state(), RobotPlcState::kReady);

  controller.handleCommandRegister(0U);

  EXPECT_EQ(controller.state(), RobotPlcState::kIdle);
  EXPECT_EQ(controller.pendingTriggerRequests(), 0);
  EXPECT_EQ(controller.registers()[RegisterLayout::kCmd], 0U);
  for (std::size_t index = RegisterLayout::kPosition;
       index < RegisterLayout::kRegisterCount; ++index) {
    EXPECT_EQ(controller.registers()[index], 0U);
  }
}

TEST(RobotPlcStateMachineTest,
     secondStartRequiresMasterResetBeforeNextRisingEdge) {
  RobotPlcController controller;

  controller.handleCommandRegister(1U);
  controller.handleTriggerResponse(true);
  ASSERT_TRUE(controller.applyTopicPayload(
      R"({"position":{"x":1.0,"y":2.0,"z":3.0},"attitude":{"roll":4.0,"pitch":5.0,"yaw":6.0},"confidence":0.42})"));
  ASSERT_EQ(controller.state(), RobotPlcState::kReady);

  controller.handleCommandRegister(1U);

  EXPECT_EQ(controller.state(), RobotPlcState::kReady);
  EXPECT_EQ(controller.pendingTriggerRequests(), 0);

  controller.handleCommandRegister(0U);
  controller.handleCommandRegister(1U);

  EXPECT_EQ(controller.state(), RobotPlcState::kWaiting);
  EXPECT_EQ(controller.pendingTriggerRequests(), 1);
}

}  // namespace
}  // namespace robot_plc_crontorl
