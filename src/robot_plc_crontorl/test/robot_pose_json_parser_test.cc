#include <gtest/gtest.h>

#include "robot_plc_crontorl/robot_pose_payload.h"

namespace robot_plc_crontorl {
namespace {

TEST(RobotPoseJsonParserTest, parsesValidPayload) {
  const auto parsed = parseRobotPosePayload(
      R"({"position":{"x":1.5,"y":2.5,"z":3.5},"attitude":{"roll":4.5,"pitch":5.5,"yaw":6.5},"confidence":0.85})");

  ASSERT_TRUE(parsed.has_value());
  EXPECT_FLOAT_EQ(parsed->x, 1.5F);
  EXPECT_FLOAT_EQ(parsed->y, 2.5F);
  EXPECT_FLOAT_EQ(parsed->z, 3.5F);
  EXPECT_FLOAT_EQ(parsed->roll, 4.5F);
  EXPECT_FLOAT_EQ(parsed->pitch, 5.5F);
  EXPECT_FLOAT_EQ(parsed->yaw, 6.5F);
  EXPECT_FLOAT_EQ(parsed->confidence, 0.85F);
}

TEST(RobotPoseJsonParserTest, rejectsMissingAttitude) {
  const auto parsed =
      parseRobotPosePayload(R"({"position":{"x":1.0,"y":2.0,"z":3.0}})");

  EXPECT_FALSE(parsed.has_value());
}

TEST(RobotPoseJsonParserTest, rejectsMissingConfidence) {
  const auto parsed = parseRobotPosePayload(
      R"({"position":{"x":1.0,"y":2.0,"z":3.0},"attitude":{"roll":4.0,"pitch":5.0,"yaw":6.0}})");

  EXPECT_FALSE(parsed.has_value());
}

}  // namespace
}  // namespace robot_plc_crontorl
