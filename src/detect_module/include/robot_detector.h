#ifndef ROBOT_DETECTOR_H
#define ROBOT_DETECTOR_H

#include <pcl/common/centroid.h>
#include <pcl/common/common.h>
#include <pcl/common/pca.h>
#include <pcl/features/esf.h>
#include <pcl/features/fpfh.h>
#include <pcl/features/gasd.h>
#include <pcl/features/normal_3d.h>
#include <pcl/features/vfh.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/filters/filter.h>
#include <pcl/filters/statistical_outlier_removal.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/io/pcd_io.h>
#include <pcl/kdtree/kdtree.h>
#include <pcl/point_types.h>
#include <pcl/search/kdtree.h>
#include <pcl/segmentation/extract_clusters.h>
#include <pcl/segmentation/region_growing.h>
#include <pcl/segmentation/sac_segmentation.h>
#include <pcl/segmentation/supervoxel_clustering.h>
#include <pcl_conversions/pcl_conversions.h>

#include <Eigen/Core>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <geometry_msgs/msg/point.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <iomanip>
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>
#include <numeric>
#include <random>
#include <rcl_interfaces/msg/set_parameters_result.hpp>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <sstream>
#include <std_msgs/msg/float32.hpp>
#include <std_msgs/msg/string.hpp>
#include <std_srvs/srv/trigger.hpp>
#include <string>
#include <vector>

/**
 * @brief 机器人检测器类
 *
 * 从配置文件初始化，加载模板特征和尺寸范围，
 * 对输入点云进行预处理、聚类、尺寸筛选和特征匹配，
 * 返回最佳匹配的机器人点云、质心和置信度。
 */
class RobotDetector : public rclcpp::Node {
   public:
    /**
     * @brief 构造函数，传入配置文件路径
     * @param config_file JSON格式的配置文件路径
     */
    explicit RobotDetector(const std::string& config_path);

    /**
     * @brief 析构函数
     */
    ~RobotDetector() = default;

   private:
    // 话题回调函数
    void pointcloud_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg);
    /**
     * @brief 检测机器人（主接口）
     * @param input_cloud 输入场景点云
     * @param result_cloud 输出机器人点云簇
     * @param centroid 输出质心 (x,y,z,1)
     * @param confidence 输出置信度 (0~1)
     * @return true 检测成功，false 失败
     */
    bool detect(const pcl::PointCloud<pcl::PointXYZ>::Ptr& input_cloud, pcl::PointCloud<pcl::PointXYZ>::Ptr& result_cloud,
                std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr>& cluster_clouds, Eigen::Vector4f& centroid, float& confidence);

    /**
     * @brief 加载配置文件
     * @return 成功返回true
     */
    bool load_config();
    /**
     * @brief 加载模板特征
     * @return 成功返回true
     */
    bool load_templates();

    /**
     * @brief 加载点云数量范围
     * @return 成功返回true
     */
    bool load_points_number_ranges();

    /**
     * @brief 输出配置信息
     */
    void print_config() const;

    /**
     * @brief 计算平均FPFH特征
     * @param cloud 输入点云
     * @param feature 输出33维特征
     * @return 成功返回true
     */
    bool compute_avg_fpfh(const pcl::PointCloud<pcl::PointXYZ>::Ptr& cloud, std::vector<float>& feature) const;

    /**
     * @brief 计算两个特征的欧氏距离
     */
    float compute_euclidean_distance(const std::vector<float>& a, const std::vector<float>& b) const;

    /**
     * @brief 将距离转换为置信度 (0~1)
     * @param distance 欧氏距离
     * @return 置信度，距离越小置信度越高
     */
    float distance_to_confidence(float distance) const;

    /**
     * @brief 点云预处理（降采样、去噪、去地面）
     * @param cloud 输入点云
     * @return 处理后的点云
     */
    pcl::PointCloud<pcl::PointXYZ>::Ptr preprocess(const pcl::PointCloud<pcl::PointXYZ>::Ptr& cloud) const;

    /**
     * @brief 条件欧几里得聚类
     * @param cloud 预处理后的点云
     * @return 聚类簇列表
     */
    std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr> region_growing(const pcl::PointCloud<pcl::PointXYZ>::Ptr& cloud) const;

    /**
     * @brief 计算质心
     */
    Eigen::Vector4f compute_centroid(const pcl::PointCloud<pcl::PointXYZ>::Ptr& cluster) const;

    /**
     * @brief 将聚类结果转换为彩色点云
     * @param clusters 聚类簇列表
     * @param frame_id 点云坐标系
     * @return 彩色点云
     */
    sensor_msgs::msg::PointCloud2 convertClustersToColoredPointCloud(const std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr>& clusters,
                                                                     const std::string& frame_id);
    /**
     * @brief 保存聚类结果
     * @param clusters 聚类簇列表
     * @param root_folder 保存路径
     */
    void save_clusters(const std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr>& clusters, const std::string& root_folder);

    /**
     * @brief 使用PCA去除直线噪声
     * @param cloud 输入点云
     * @param threshold_multiplier 阈值倍数
     * @return 去除直线噪声后的点云
     */
    pcl::PointCloud<pcl::PointXYZ>::Ptr remove_lines_using_pca(const pcl::PointCloud<pcl::PointXYZ>::Ptr& cloud,
                                                               double threshold_multiplier = 2.5);

   private:
    // 基本参数
    std::string config_path_;                    // 配置文件路径
    std::vector<std::vector<float>> templates_;  // 模板特征

    // 接收话题
    rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr pointcloud_sub_;  // 点云订阅器
    // 发布话题
    rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr cluster_pointcloud_pub_;  // 机聚类点云发布器
    rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr robot_pointcloud_pub_;    // 机器人点云发布器
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr robot_info_pub_;                  // 机器人置信度与质心发布器
    rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr start_detection_srv_;              // 启动检测服务

    // 记录最新质心与置信度
    std::atomic<std::chrono::time_point<std::chrono::system_clock, std::chrono::system_clock::duration>> latest_detection_time_;
    std::atomic<float> latest_x_;
    std::atomic<float> latest_y_;
    std::atomic<float> latest_z_;
    std::atomic<float> latest_confidence_;

    // 点云数量范围
    std::vector<int> points_number_ranges_;
};

#endif  // ROBOT_DETECTOR_H