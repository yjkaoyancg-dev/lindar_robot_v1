#include <chrono>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <std_msgs/msg/bool.hpp>
#include <std_msgs/msg/float32.hpp>
#include <std_msgs/msg/string.hpp>
#include <std_srvs/srv/trigger.hpp>

using json = nlohmann::json;
using namespace std::chrono_literals;

class RangePlcBridgeNode : public rclcpp::Node {
public:
    RangePlcBridgeNode() : Node("range_plc_bridge_node") {
        pose_topic_ = declare_parameter<std::string>("pose_topic", "/robot_info_range");
        detected_topic_ = declare_parameter<std::string>("detected_topic", "/range_detector/detected");
        confidence_topic_ = declare_parameter<std::string>("confidence_topic", "/range_detector/confidence");
        robot_info_topic_ = declare_parameter<std::string>("robot_info_topic", "/robot_info");
        service_name_ = declare_parameter<std::string>("service_name", "/start_detection");

        timeout_ms_ = declare_parameter<int>("timeout_ms", 2000);
        freshness_ms_ = declare_parameter<int>("freshness_ms", 500);
        zero_on_timeout_ = declare_parameter<bool>("zero_on_timeout", true);
        use_latest_if_fresh_ = declare_parameter<bool>("use_latest_if_fresh", true);

        pose_sub_ = create_subscription<geometry_msgs::msg::PoseStamped>(
            pose_topic_, 10,
            std::bind(&RangePlcBridgeNode::poseCallback, this, std::placeholders::_1));

        detected_sub_ = create_subscription<std_msgs::msg::Bool>(
            detected_topic_, 10,
            std::bind(&RangePlcBridgeNode::detectedCallback, this, std::placeholders::_1));

        confidence_sub_ = create_subscription<std_msgs::msg::Float32>(
            confidence_topic_, 10,
            std::bind(&RangePlcBridgeNode::confidenceCallback, this, std::placeholders::_1));

        robot_info_pub_ = create_publisher<std_msgs::msg::String>(robot_info_topic_, 10);

        trigger_srv_ = create_service<std_srvs::srv::Trigger>(
            service_name_,
            std::bind(&RangePlcBridgeNode::triggerCallback, this,
                      std::placeholders::_1, std::placeholders::_2));

        timer_ = create_wall_timer(
            50ms, std::bind(&RangePlcBridgeNode::timerCallback, this));

        RCLCPP_INFO(get_logger(), "range_plc_bridge_node 已启动");
        RCLCPP_INFO(get_logger(), "pose_topic=%s", pose_topic_.c_str());
        RCLCPP_INFO(get_logger(), "detected_topic=%s", detected_topic_.c_str());
        RCLCPP_INFO(get_logger(), "confidence_topic=%s", confidence_topic_.c_str());
        RCLCPP_INFO(get_logger(), "robot_info_topic=%s", robot_info_topic_.c_str());
        RCLCPP_INFO(get_logger(), "service_name=%s", service_name_.c_str());
    }

private:
    void poseCallback(const geometry_msgs::msg::PoseStamped::SharedPtr msg) {
        last_pose_ = *msg;
        last_pose_time_ = now();
        tryPublishIfArmed();
    }

    void detectedCallback(const std_msgs::msg::Bool::SharedPtr msg) {
        last_detected_ = msg->data;
        last_detected_time_ = now();
        tryPublishIfArmed();
    }

    void confidenceCallback(const std_msgs::msg::Float32::SharedPtr msg) {
        last_confidence_ = msg->data;
        last_confidence_time_ = now();
        tryPublishIfArmed();
    }

    void triggerCallback(
        const std::shared_ptr<std_srvs::srv::Trigger::Request> /*request*/,
        std::shared_ptr<std_srvs::srv::Trigger::Response> response) {

        armed_ = true;
        deadline_ = now() + rclcpp::Duration::from_seconds(timeout_ms_ / 1000.0);

        RCLCPP_INFO(get_logger(), "收到 /start_detection 请求，开始等待新检测结果");

        if (use_latest_if_fresh_ && latestDataFresh() && last_detected_) {
            publishRobotInfo(true);
            armed_ = false;
            response->success = true;
            response->message = "Detection started and latest fresh result published immediately.";
            return;
        }

        response->success = true;
        response->message = "Detection trigger accepted, waiting for result.";
    }

    void timerCallback() {
        if (!armed_) {
            return;
        }

        if (latestDataFresh() && last_detected_) {
            publishRobotInfo(true);
            armed_ = false;
            return;
        }

        if (now() >= deadline_) {
            RCLCPP_WARN(get_logger(), "等待检测结果超时");
            if (zero_on_timeout_) {
                publishRobotInfo(false);
            }
            armed_ = false;
        }
    }

    void tryPublishIfArmed() {
        if (!armed_) {
            return;
        }

        if (latestDataFresh() && last_detected_) {
            publishRobotInfo(true);
            armed_ = false;
        }
    }

    bool latestDataFresh() const {
        const auto t = now();
        const auto max_age = rclcpp::Duration::from_seconds(freshness_ms_ / 1000.0);

        if ((t - last_pose_time_) > max_age) return false;
        if ((t - last_detected_time_) > max_age) return false;
        if ((t - last_confidence_time_) > max_age) return false;
        return true;
    }

    void publishRobotInfo(bool detected) {
        json j;

        double x = 0.0, y = 0.0, z = 0.0;
        double roll = 0.0, pitch = 0.0, yaw = 0.0;
        double confidence = 0.0;

        if (detected) {
            x = last_pose_.pose.position.x;
            y = last_pose_.pose.position.y;
            z = last_pose_.pose.position.z;
            confidence = static_cast<double>(last_confidence_);
        }
        j["position"] = {
            {"x", x},
            {"y", y},
            {"z", z}
        };

        j["attitude"] = {
            {"roll", roll},
            {"pitch", pitch},
            {"yaw", yaw}
        };

        j["confidence"] = confidence;
std_msgs::msg::String msg;
        msg.data = j.dump();
        robot_info_pub_->publish(msg);

        RCLCPP_INFO(
            get_logger(),
            "发布 /robot_info: detected=%d, pos=(%.4f, %.4f, %.4f), conf=%.3f",
            detected ? 1 : 0, x, y, z, confidence);
    }

private:
    std::string pose_topic_;
    std::string detected_topic_;
    std::string confidence_topic_;
    std::string robot_info_topic_;
    std::string service_name_;

    int timeout_ms_{2000};
    int freshness_ms_{500};
    bool zero_on_timeout_{true};
    bool use_latest_if_fresh_{true};

    bool armed_{false};
    rclcpp::Time deadline_{0, 0, RCL_ROS_TIME};

    geometry_msgs::msg::PoseStamped last_pose_;
    bool last_detected_{false};
    float last_confidence_{0.0f};

    rclcpp::Time last_pose_time_{0, 0, RCL_ROS_TIME};
    rclcpp::Time last_detected_time_{0, 0, RCL_ROS_TIME};
    rclcpp::Time last_confidence_time_{0, 0, RCL_ROS_TIME};

    rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr pose_sub_;
    rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr detected_sub_;
    rclcpp::Subscription<std_msgs::msg::Float32>::SharedPtr confidence_sub_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr robot_info_pub_;
    rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr trigger_srv_;
    rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<RangePlcBridgeNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
