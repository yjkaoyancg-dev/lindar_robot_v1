#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <numeric>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <std_msgs/msg/bool.hpp>
#include <std_msgs/msg/float32.hpp>
#include <visualization_msgs/msg/marker.hpp>
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/filters/passthrough.h>
#include <pcl/filters/voxel_grid.h>

using json = nlohmann::json;

struct RangeDensityConfig {
    std::string topic_name = "/pointcloud_BFZ9IEC9";
    std::string output_pose_topic = "/robot_info_range";
    std::string output_detected_topic = "/range_detector/detected";
    std::string output_confidence_topic = "/range_detector/confidence";
    std::string output_target_cloud_topic = "/range_detector/target_cloud";
    std::string output_marker_topic = "/range_detector/marker";

    bool publish_target_cloud = true;
    bool publish_marker = true;

    std::string scan_axis = "x";    // x / y / z
    bool near_to_far = true;        // true: 小到大扫描, false: 大到小扫描

    float voxel_size = 0.01f;

    float roi_x_min = -10.0f;
    float roi_x_max = 10.0f;
    float roi_y_min = -10.0f;
    float roi_y_max = 10.0f;
    float roi_z_min = -10.0f;
    float roi_z_max = 10.0f;

    float bin_min = 0.0f;
    float bin_max = 3.0f;
    float bin_size = 0.03f;

    int smooth_window = 3;          // 建议奇数
    int min_points_per_bin = 20;
    int min_consecutive_bins = 3;
    int max_consecutive_bins = 20;

    float max_y_std = 0.20f;
    float max_z_std = 0.20f;

    int min_total_points = 80;
    int confidence_ref_points = 800;

    bool save_debug_profile = false;
    std::string debug_profile_path = "/tmp/range_density_profile.csv";
};

class RangeDensityDetectorNode : public rclcpp::Node {
public:
    explicit RangeDensityDetectorNode(const rclcpp::NodeOptions& options = rclcpp::NodeOptions())
        : Node("range_density_detector_node", options) {
        const std::string config_path = declare_parameter<std::string>(
            "config_path", "/opt/zwkj/configs/range_density_detect_config.json");

        loadConfig(config_path);

        pointcloud_sub_ = create_subscription<sensor_msgs::msg::PointCloud2>(
            cfg_.topic_name, rclcpp::SensorDataQoS(),
            std::bind(&RangeDensityDetectorNode::pointCloudCallback, this, std::placeholders::_1));

        pose_pub_ = create_publisher<geometry_msgs::msg::PoseStamped>(cfg_.output_pose_topic, 10);
        detected_pub_ = create_publisher<std_msgs::msg::Bool>(cfg_.output_detected_topic, 10);
        confidence_pub_ = create_publisher<std_msgs::msg::Float32>(cfg_.output_confidence_topic, 10);

        if (cfg_.publish_target_cloud) {
            target_cloud_pub_ = create_publisher<sensor_msgs::msg::PointCloud2>(cfg_.output_target_cloud_topic, 10);
        }
        if (cfg_.publish_marker) {
            marker_pub_ = create_publisher<visualization_msgs::msg::Marker>(cfg_.output_marker_topic, 10);
        }

        RCLCPP_INFO(get_logger(), "范围密度检测节点已启动");
        RCLCPP_INFO(get_logger(), "配置文件路径: %s", config_path.c_str());
        RCLCPP_INFO(get_logger(), "点云话题名称: %s", cfg_.topic_name.c_str());
        RCLCPP_INFO(get_logger(), "扫描轴: %s", cfg_.scan_axis.c_str());
        RCLCPP_INFO(get_logger(), "扫描方向: %s", cfg_.near_to_far ? "近到远" : "远到近");
        RCLCPP_INFO(get_logger(), "ROI: x[%.3f, %.3f], y[%.3f, %.3f], z[%.3f, %.3f]",
                    cfg_.roi_x_min, cfg_.roi_x_max,
                    cfg_.roi_y_min, cfg_.roi_y_max,
                    cfg_.roi_z_min, cfg_.roi_z_max);
        RCLCPP_INFO(get_logger(), "Bin: [%.3f, %.3f], size=%.3f",
                    cfg_.bin_min, cfg_.bin_max, cfg_.bin_size);
    }

private:
    struct DetectionResult {
        bool detected = false;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float confidence = 0.0f;
        pcl::PointCloud<pcl::PointXYZ>::Ptr target_cloud{new pcl::PointCloud<pcl::PointXYZ>()};
    };

    void loadConfig(const std::string& path) {
        std::ifstream ifs(path);
        if (!ifs.is_open()) {
            throw std::runtime_error("无法打开配置文件: " + path);
        }

        json j;
        ifs >> j;

        auto get_if_exists = [&](auto& target, const char* key) {
            if (j.contains(key)) {
                target = j.at(key).get<std::decay_t<decltype(target)>>();
            }
        };

        get_if_exists(cfg_.topic_name, "topic_name");
        get_if_exists(cfg_.output_pose_topic, "output_pose_topic");
        get_if_exists(cfg_.output_detected_topic, "output_detected_topic");
        get_if_exists(cfg_.output_confidence_topic, "output_confidence_topic");
        get_if_exists(cfg_.output_target_cloud_topic, "output_target_cloud_topic");
        get_if_exists(cfg_.output_marker_topic, "output_marker_topic");

        get_if_exists(cfg_.publish_target_cloud, "publish_target_cloud");
        get_if_exists(cfg_.publish_marker, "publish_marker");

        get_if_exists(cfg_.scan_axis, "scan_axis");
        get_if_exists(cfg_.near_to_far, "near_to_far");

        get_if_exists(cfg_.voxel_size, "voxel_size");

        get_if_exists(cfg_.roi_x_min, "roi_x_min");
        get_if_exists(cfg_.roi_x_max, "roi_x_max");
        get_if_exists(cfg_.roi_y_min, "roi_y_min");
        get_if_exists(cfg_.roi_y_max, "roi_y_max");
        get_if_exists(cfg_.roi_z_min, "roi_z_min");
        get_if_exists(cfg_.roi_z_max, "roi_z_max");

        get_if_exists(cfg_.bin_min, "bin_min");
        get_if_exists(cfg_.bin_max, "bin_max");
        get_if_exists(cfg_.bin_size, "bin_size");

        get_if_exists(cfg_.smooth_window, "smooth_window");
        get_if_exists(cfg_.min_points_per_bin, "min_points_per_bin");
        get_if_exists(cfg_.min_consecutive_bins, "min_consecutive_bins");
        get_if_exists(cfg_.max_consecutive_bins, "max_consecutive_bins");

        get_if_exists(cfg_.max_y_std, "max_y_std");
        get_if_exists(cfg_.max_z_std, "max_z_std");
        get_if_exists(cfg_.min_total_points, "min_total_points");
        get_if_exists(cfg_.confidence_ref_points, "confidence_ref_points");

        get_if_exists(cfg_.save_debug_profile, "save_debug_profile");
        get_if_exists(cfg_.debug_profile_path, "debug_profile_path");

        if (cfg_.scan_axis != "x" && cfg_.scan_axis != "y" && cfg_.scan_axis != "z") {
            throw std::runtime_error("scan_axis 只能是 x / y / z");
        }
        if (cfg_.bin_size <= 0.0f) {
            throw std::runtime_error("bin_size 必须大于 0");
        }
        if (cfg_.bin_max <= cfg_.bin_min) {
            throw std::runtime_error("bin_max 必须大于 bin_min");
        }
        if (cfg_.smooth_window < 1) {
            cfg_.smooth_window = 1;
        }
        if (cfg_.smooth_window % 2 == 0) {
            cfg_.smooth_window += 1;
        }
    }

    void pointCloudCallback(const sensor_msgs::msg::PointCloud2::SharedPtr msg) {
        pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>());
        pcl::fromROSMsg(*msg, *cloud);

        if (cloud->empty()) {
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 3000, "收到空点云");
            publishNoTarget(msg->header);
            return;
        }

        auto filtered = preprocess(cloud);
        auto result = detectByRangeDensity(filtered);

        publishResult(msg->header, result);
    }

    pcl::PointCloud<pcl::PointXYZ>::Ptr preprocess(const pcl::PointCloud<pcl::PointXYZ>::Ptr& cloud) {
        pcl::PointCloud<pcl::PointXYZ>::Ptr filtered(new pcl::PointCloud<pcl::PointXYZ>(*cloud));

        // ROI passthrough
        filtered = passThrough(filtered, "x", cfg_.roi_x_min, cfg_.roi_x_max);
        filtered = passThrough(filtered, "y", cfg_.roi_y_min, cfg_.roi_y_max);
        filtered = passThrough(filtered, "z", cfg_.roi_z_min, cfg_.roi_z_max);

        // voxel
        if (cfg_.voxel_size > 0.0f) {
            pcl::VoxelGrid<pcl::PointXYZ> vg;
            vg.setInputCloud(filtered);
            vg.setLeafSize(cfg_.voxel_size, cfg_.voxel_size, cfg_.voxel_size);
            pcl::PointCloud<pcl::PointXYZ>::Ptr downsampled(new pcl::PointCloud<pcl::PointXYZ>());
            vg.filter(*downsampled);
            filtered = downsampled;
        }

        return filtered;
    }

    pcl::PointCloud<pcl::PointXYZ>::Ptr passThrough(
        const pcl::PointCloud<pcl::PointXYZ>::Ptr& cloud,
        const std::string& field_name,
        float min_v,
        float max_v) {
        pcl::PassThrough<pcl::PointXYZ> pass;
        pass.setInputCloud(cloud);
        pass.setFilterFieldName(field_name);
        pass.setFilterLimits(min_v, max_v);
        pcl::PointCloud<pcl::PointXYZ>::Ptr out(new pcl::PointCloud<pcl::PointXYZ>());
        pass.filter(*out);
        return out;
    }

    float axisValue(const pcl::PointXYZ& p) const {
        if (cfg_.scan_axis == "x") return p.x;
        if (cfg_.scan_axis == "y") return p.y;
        return p.z;
    }

    DetectionResult detectByRangeDensity(const pcl::PointCloud<pcl::PointXYZ>::Ptr& cloud) {
        DetectionResult result;

        if (cloud->empty()) {
            return result;
        }

        const int num_bins = static_cast<int>(std::floor((cfg_.bin_max - cfg_.bin_min) / cfg_.bin_size)) + 1;
        if (num_bins <= 0) {
            return result;
        }

        std::vector<int> bins(num_bins, 0);

        for (const auto& p : cloud->points) {
            const float v = axisValue(p);
            if (v < cfg_.bin_min || v >= cfg_.bin_max) continue;
            const int idx = static_cast<int>((v - cfg_.bin_min) / cfg_.bin_size);
            if (idx >= 0 && idx < num_bins) {
                bins[idx]++;
            }
        }

        std::vector<float> smooth(num_bins, 0.0f);
        const int half = cfg_.smooth_window / 2;
        for (int i = 0; i < num_bins; ++i) {
            float sum = 0.0f;
            int cnt = 0;
            for (int k = i - half; k <= i + half; ++k) {
                if (k >= 0 && k < num_bins) {
                    sum += static_cast<float>(bins[k]);
                    cnt++;
                }
            }
            smooth[i] = (cnt > 0) ? sum / static_cast<float>(cnt) : 0.0f;
        }

        if (cfg_.save_debug_profile) {
            saveProfile(bins, smooth);
        }

        int start_idx = -1;
        int end_idx = -1;
        int current_start = -1;
        int current_len = 0;

        auto accept_segment = [&](int len) {
            return len >= cfg_.min_consecutive_bins && len <= cfg_.max_consecutive_bins;
        };

        auto scan_bin = [&](int i) {
            if (smooth[i] >= static_cast<float>(cfg_.min_points_per_bin)) {
                if (current_start < 0) current_start = i;
                current_len++;
            } else {
                if (current_start >= 0 && accept_segment(current_len)) {
                    start_idx = current_start;
                    end_idx = (cfg_.near_to_far ? i - 1 : i + 1);
                    return true;
                }
                current_start = -1;
                current_len = 0;
            }
            return false;
        };

        if (cfg_.near_to_far) {
            for (int i = 0; i < num_bins; ++i) {
                if (scan_bin(i)) break;
            }
            if (start_idx < 0 && current_start >= 0 && accept_segment(current_len)) {
                start_idx = current_start;
                end_idx = num_bins - 1;
            }
        } else {
            current_start = -1;
            current_len = 0;
            for (int i = num_bins - 1; i >= 0; --i) {
                if (smooth[i] >= static_cast<float>(cfg_.min_points_per_bin)) {
                    if (current_start < 0) current_start = i;
                    current_len++;
                } else {
                    if (current_start >= 0 && accept_segment(current_len)) {
                        end_idx = current_start;
                        start_idx = i + 1;
                        break;
                    }
                    current_start = -1;
                    current_len = 0;
                }
            }
            if (start_idx < 0 && current_start >= 0 && accept_segment(current_len)) {
                start_idx = 0;
                end_idx = current_start;
            }
        }

        if (start_idx < 0 || end_idx < 0 || end_idx < start_idx) {
            RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 3000, "未找到满足条件的高密度距离段");
            return result;
        }

        const float seg_min = cfg_.bin_min + static_cast<float>(start_idx) * cfg_.bin_size;
        const float seg_max = cfg_.bin_min + static_cast<float>(end_idx + 1) * cfg_.bin_size;

        for (const auto& p : cloud->points) {
            const float v = axisValue(p);
            if (v >= seg_min && v < seg_max) {
                result.target_cloud->points.push_back(p);
            }
        }

        if (static_cast<int>(result.target_cloud->size()) < cfg_.min_total_points) {
            RCLCPP_INFO_THROTTLE(
                get_logger(), *get_clock(), 3000,
                "距离段点数不足: %zu < %d", result.target_cloud->size(), cfg_.min_total_points);
            return result;
        }

        float mx = 0.0f, my = 0.0f, mz = 0.0f;
        for (const auto& p : result.target_cloud->points) {
            mx += p.x;
            my += p.y;
            mz += p.z;
        }
        mx /= static_cast<float>(result.target_cloud->size());
        my /= static_cast<float>(result.target_cloud->size());
        mz /= static_cast<float>(result.target_cloud->size());

        float var_y = 0.0f, var_z = 0.0f;
        for (const auto& p : result.target_cloud->points) {
            var_y += (p.y - my) * (p.y - my);
            var_z += (p.z - mz) * (p.z - mz);
        }
        var_y /= static_cast<float>(result.target_cloud->size());
        var_z /= static_cast<float>(result.target_cloud->size());

        const float std_y = std::sqrt(var_y);
        const float std_z = std::sqrt(var_z);

        if (std_y > cfg_.max_y_std || std_z > cfg_.max_z_std) {
            RCLCPP_INFO_THROTTLE(
                get_logger(), *get_clock(), 3000,
                "目标段横向/高度离散度过大: std_y=%.4f, std_z=%.4f", std_y, std_z);
            return result;
        }

        result.detected = true;
        result.x = mx;
        result.y = my;
        result.z = mz;
        result.confidence = std::min(
            1.0f, static_cast<float>(result.target_cloud->size()) / static_cast<float>(cfg_.confidence_ref_points));

        RCLCPP_INFO(
            get_logger(),
            "检测到目标: segment=[%.3f, %.3f), 点数=%zu, centroid=(%.4f, %.4f, %.4f), conf=%.3f",
            seg_min, seg_max, result.target_cloud->size(), result.x, result.y, result.z, result.confidence);

        return result;
    }

    void saveProfile(const std::vector<int>& bins, const std::vector<float>& smooth) {
        std::filesystem::path path(cfg_.debug_profile_path);
        if (!path.parent_path().empty()) {
            std::filesystem::create_directories(path.parent_path());
        }

        std::ofstream ofs(path);
        ofs << "bin_center,count,smooth\n";
        for (size_t i = 0; i < bins.size(); ++i) {
            const float center = cfg_.bin_min + (static_cast<float>(i) + 0.5f) * cfg_.bin_size;
            ofs << center << "," << bins[i] << "," << smooth[i] << "\n";
        }
    }

    void publishNoTarget(const std_msgs::msg::Header& header) {
        std_msgs::msg::Bool detected_msg;
        detected_msg.data = false;
        detected_pub_->publish(detected_msg);

        std_msgs::msg::Float32 confidence_msg;
        confidence_msg.data = 0.0f;
        confidence_pub_->publish(confidence_msg);

        geometry_msgs::msg::PoseStamped pose_msg;
        pose_msg.header = header;
        pose_msg.pose.position.x = 0.0;
        pose_msg.pose.position.y = 0.0;
        pose_msg.pose.position.z = 0.0;
        pose_msg.pose.orientation.w = 1.0;
        pose_pub_->publish(pose_msg);

        if (cfg_.publish_marker && marker_pub_) {
            visualization_msgs::msg::Marker marker;
            marker.header = header;
            marker.ns = "range_density_detector";
            marker.id = 0;
            marker.type = visualization_msgs::msg::Marker::SPHERE;
            marker.action = visualization_msgs::msg::Marker::DELETE;
            marker_pub_->publish(marker);
        }
    }

    void publishResult(const std_msgs::msg::Header& header, const DetectionResult& result) {
        if (!result.detected) {
            publishNoTarget(header);
            return;
        }

        std_msgs::msg::Bool detected_msg;
        detected_msg.data = true;
        detected_pub_->publish(detected_msg);

        std_msgs::msg::Float32 confidence_msg;
        confidence_msg.data = result.confidence;
        confidence_pub_->publish(confidence_msg);

        geometry_msgs::msg::PoseStamped pose_msg;
        pose_msg.header = header;
        pose_msg.pose.position.x = result.x;
        pose_msg.pose.position.y = result.y;
        pose_msg.pose.position.z = result.z;
        pose_msg.pose.orientation.w = 1.0;
        pose_pub_->publish(pose_msg);

        if (cfg_.publish_target_cloud && target_cloud_pub_) {
            sensor_msgs::msg::PointCloud2 out_cloud;
            pcl::toROSMsg(*(result.target_cloud), out_cloud);
            out_cloud.header = header;
            target_cloud_pub_->publish(out_cloud);
        }

        if (cfg_.publish_marker && marker_pub_) {
            visualization_msgs::msg::Marker marker;
            marker.header = header;
            marker.ns = "range_density_detector";
            marker.id = 0;
            marker.type = visualization_msgs::msg::Marker::SPHERE;
            marker.action = visualization_msgs::msg::Marker::ADD;
            marker.pose.position.x = result.x;
            marker.pose.position.y = result.y;
            marker.pose.position.z = result.z;
            marker.pose.orientation.w = 1.0;
            marker.scale.x = 0.08;
            marker.scale.y = 0.08;
            marker.scale.z = 0.08;
            marker.color.a = 1.0;
            marker.color.r = 1.0;
            marker.color.g = 0.0;
            marker.color.b = 0.0;
            marker_pub_->publish(marker);
        }
    }

private:
    RangeDensityConfig cfg_;

    rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr pointcloud_sub_;
    rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr pose_pub_;
    rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr detected_pub_;
    rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr confidence_pub_;
    rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr target_cloud_pub_;
    rclcpp::Publisher<visualization_msgs::msg::Marker>::SharedPtr marker_pub_;
};

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    try {
        auto node = std::make_shared<RangeDensityDetectorNode>();
        rclcpp::spin(node);
    } catch (const std::exception& e) {
        std::cerr << "range_density_detector_node 启动失败: " << e.what() << std::endl;
    }
    rclcpp::shutdown();
    return 0;
}