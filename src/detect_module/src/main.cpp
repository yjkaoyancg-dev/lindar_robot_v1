#include <rclcpp/rclcpp.hpp>

#include "robot_detector.h"

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    try {
        auto node = std::make_shared<RobotDetector>("/opt/zwkj/configs/detect_config.json");
        rclcpp::spin(node);
    } catch (const std::exception& e) {
        RCLCPP_ERROR(rclcpp::get_logger("main"), "Exception: %s", e.what());
        return 1;
    }
    rclcpp::shutdown();
    return 0;
}
