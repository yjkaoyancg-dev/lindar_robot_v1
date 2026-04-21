#include "robot_detector.h"

/**
 * @brief 构造函数，传入配置文件路径
 * @param config_file JSON格式的配置文件路径
 */
RobotDetector::RobotDetector(const std::string& config_path) : Node("robot_detector_node"), config_path_(config_path) {
    // 加载配置文件
    load_config();
    // 加载点云数量范围
    // 加载模板特征
    if (load_points_number_ranges() && load_templates()) {
        print_config();
        // 获取订阅话题名称
        std::string topic_name = this->get_parameter("topic_name").as_string();
        // 订阅点云话题
        pointcloud_sub_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
            topic_name, rclcpp::SensorDataQoS(), std::bind(&RobotDetector::pointcloud_callback, this, std::placeholders::_1));
        // 发布聚类点云
        cluster_pointcloud_pub_ = this->create_publisher<sensor_msgs::msg::PointCloud2>("/cluster_pointcloud", 10);
        // 发布机器人点云
        robot_pointcloud_pub_ = this->create_publisher<sensor_msgs::msg::PointCloud2>("/robot_pointcloud", 10);
        // 发布机器人质心置信度
        robot_info_pub_ = this->create_publisher<std_msgs::msg::String>("/robot_info", 10);
        // 开启服务
        start_detection_srv_ = this->create_service<std_srvs::srv::Trigger>(
            "/start_detection",
            [this](const std::shared_ptr<std_srvs::srv::Trigger::Request>,
                   std::shared_ptr<std_srvs::srv::Trigger::Response> response) -> void {
                RCLCPP_INFO(this->get_logger(), "收到启动检测请求");

                std::thread get_reboot_info([this]() {
                    RCLCPP_INFO(this->get_logger(), "检测线程已启动");
                    auto now_time = std::chrono::system_clock::now();
                    while (true) {
                        if (latest_detection_time_.load(std::memory_order_acquire) - now_time >= std::chrono::milliseconds(5)) break;
                        std::this_thread::sleep_for(std::chrono::seconds(5));
                    }
                    // 方法1：输出时间戳（毫秒数）
                    auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now_time).time_since_epoch().count();
                    auto latest_ms =
                        std::chrono::time_point_cast<std::chrono::milliseconds>(latest_detection_time_.load(std::memory_order_acquire))
                            .time_since_epoch()
                            .count();

                    RCLCPP_INFO(this->get_logger(), "检测开始时间: %ld ms, 数据更新时间: %ld ms", now_ms, latest_ms);

                    auto x = latest_x_.load(std::memory_order_acquire);
                    auto y = latest_y_.load(std::memory_order_acquire);
                    auto z = latest_z_.load(std::memory_order_acquire);
                    auto confidence = latest_confidence_.load(std::memory_order_acquire);

                    std::mt19937 rng(std::random_device{}());

                    float roll = std::uniform_real_distribution<float>(-M_PI, M_PI)(rng);
                    float yaw = std::uniform_real_distribution<float>(-M_PI, M_PI)(rng);
                    float pitch = std::uniform_real_distribution<float>(-M_PI, M_PI)(rng);

                    auto json = nlohmann::json::object();
                    /*
                    {
                        "position": { "x": 1.23, "y": 4.56, "z": 7.89 },
                        "attitude": { "roll": 0.1, "pitch": 0.2, "yaw": 0.3 }
                        "confidence": 0.95
                    }
                    */
                    json["position"] = {{"x", x}, {"y", y}, {"z", z}};
                    json["attitude"] = {{"roll", roll}, {"pitch", pitch}, {"yaw", yaw}};
                    json["confidence"] = confidence;
                    // 发布机器人质心置信度
                    auto robot_info_msg = std_msgs::msg::String();
                    robot_info_msg.data = json.dump();
                    robot_info_pub_->publish(robot_info_msg);
                });
                get_reboot_info.detach();

                response->success = true;
                response->message = "检测已启动";
            });

        RCLCPP_INFO(this->get_logger(), "机器人检测节点已启动");
    } else {
        exit(-1);
    }
}

void RobotDetector::pointcloud_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg) {
    bool is_pub_cluster = this->get_parameter("is_pub_clusters").as_bool();
    bool is_save_cluster = this->get_parameter("is_save_clusters").as_bool();
    std::string save_clusters_dir = this->get_parameter("save_clusters_dir").as_string();

    // 点云类型转换
    pcl::PointCloud<pcl::PointXYZ>::Ptr input_cloud(new pcl::PointCloud<pcl::PointXYZ>);
    pcl::PointCloud<pcl::PointXYZ>::Ptr output_cloud(new pcl::PointCloud<pcl::PointXYZ>);
    std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr> cluster_clouds;

    pcl::fromROSMsg(*msg, *input_cloud);

    Eigen::Vector4f centroid;
    float confidence = 0.0f;

    if (detect(input_cloud, output_cloud, cluster_clouds, centroid, confidence)) {
        RCLCPP_INFO(this->get_logger(), "检测到机器人, 置信度: %f, 质心坐标: %f, %f, %f, 点云数量: %ld", confidence, centroid.x(),
                    centroid.y(), centroid.z(), output_cloud->size());
        

        // 记录机器人质心置信度 _
        latest_detection_time_ = std::chrono::system_clock::now();
        latest_x_ = centroid.x();
        latest_y_ = centroid.y();
        latest_z_ = centroid.z();
        latest_confidence_ = confidence;

        // 发布机器人点云
        auto robot_pointcloud_msg = sensor_msgs::msg::PointCloud2();
        pcl::toROSMsg(*output_cloud, robot_pointcloud_msg);
        robot_pointcloud_msg.header = msg->header;
        robot_pointcloud_pub_->publish(robot_pointcloud_msg);
    }
    if (!cluster_clouds.empty()) {
        RCLCPP_INFO(this->get_logger(), "检测到 %ld 个聚类", cluster_clouds.size());
        if (is_pub_cluster) {
            auto cluster_ros = convertClustersToColoredPointCloud(cluster_clouds, msg->header.frame_id);
            cluster_pointcloud_pub_->publish(cluster_ros);
        }
        if (is_save_cluster) {
            save_clusters(cluster_clouds, save_clusters_dir);
        }
    }
        
}

/**
 * @brief 检测函数
 */
bool RobotDetector::detect(const pcl::PointCloud<pcl::PointXYZ>::Ptr& input_cloud, pcl::PointCloud<pcl::PointXYZ>::Ptr& result_cloud,
                           std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr>& cluster_clouds, Eigen::Vector4f& centroid, float& confidence) {
    // 初始化输出
    result_cloud.reset();
    centroid.setZero();
    confidence = 0.0f;

    float conf_threshold = this->get_parameter("templates_distance_threshold").as_double();
    int max_point_num = this->get_parameter("max_point_num").as_int();
    int min_point_num = this->get_parameter("min_point_num").as_int();
    float threshold_multiplier = this->get_parameter("pca_threshold_multiplier").as_double();

    auto cloud_filtered = preprocess(input_cloud);
    cluster_clouds = region_growing(cloud_filtered);

    int cluster_num = 1;
    for (const auto& cluster : cluster_clouds) {
        if (!cluster || cluster->empty()) continue;

        if (cluster->size() < static_cast<size_t>(min_point_num) || cluster->size() > static_cast<size_t>(max_point_num)) {
            RCLCPP_DEBUG(this->get_logger(), "聚类[%d] 点云数量不符合要求，点数: %ld", cluster_num++, cluster->size());
            continue;
        }

        std::vector<float> fpfh_features;
        if (!compute_avg_fpfh(cluster, fpfh_features)) {
            RCLCPP_DEBUG(this->get_logger(), "聚类[%d] 计算FPFH失败", cluster_num++);
            continue;
        }

        float best_cluster_conf = 0.0f;
        int template_index = 0;  // 模板索引
        for (const auto& templ : templates_) {
            float dist = compute_euclidean_distance(fpfh_features, templ);
            RCLCPP_DEBUG(this->get_logger(), "聚类[%d] 模板[%d] 距离: %f", cluster_num, template_index++, dist);
            float temp_conf = distance_to_confidence(dist);
            if (temp_conf < conf_threshold) continue;
            if (temp_conf > best_cluster_conf) best_cluster_conf = temp_conf;
        }

        RCLCPP_DEBUG(this->get_logger(), "聚类[%d] 最佳置信度: %f, 点数: %ld", cluster_num++, best_cluster_conf, cluster->size());

        if (best_cluster_conf > confidence) {
            confidence = best_cluster_conf;
            result_cloud = cluster;
        }
    }

    if (confidence > 0.0f) {
        result_cloud = remove_lines_using_pca(result_cloud, threshold_multiplier);  // 去除线
        centroid = compute_centroid(result_cloud);                                  // 重新计算质心
    }

    return result_cloud != nullptr && !result_cloud->empty();
}

/**
 * @brief 加载配置文件
 * @return 成功返回true
 */
bool RobotDetector::load_config() {
    this->declare_parameter<std::string>("config_path", config_path_);
    nlohmann::json config;
    std::ifstream ifs(config_path_);
    if (!ifs.is_open()) {
        RCLCPP_ERROR(this->get_logger(), "打开配置文件失败: %s， 将使用默认配置", config_path_.c_str());
    } else {
        ifs >> config;
        ifs.close();
    }
    // 解析配置文件
    this->declare_parameter<std::string>("topic_name", config.value("topic_name", "/pointcloud_sn"));
    this->declare_parameter<std::string>("feature_dir", config.value("feature_dir", "/opt/zwkj/templates"));
    this->declare_parameter<std::string>("save_clusters_dir", config.value("save_clusters_dir", "/opt/zwkj/clusters"));
    this->declare_parameter<float>("templates_distance_threshold", config.value("templates_distance_threshold", 0.85));
    this->declare_parameter<float>("voxel_size", config.value("voxel_size", 0.02));
    this->declare_parameter<int>("statistical_mean_k", config.value("statistical_mean_k", 50));
    this->declare_parameter<float>("statistical_stddev_mul_thresh", config.value("statistical_stddev_mul_thresh", 1.0));
    this->declare_parameter<float>("radius_search", config.value("radius_search", 0.05));
    this->declare_parameter<int>("number_of_neighbours", config.value("number_of_neighbours", 30));
    this->declare_parameter<float>("curvature_threshold", config.value("curvature_threshold", 0.05));
    this->declare_parameter<float>("smoothness_threshold", config.value("smoothness_threshold", 60.0));
    this->declare_parameter<bool>("is_pub_clusters", config.value("is_pub_clusters", false));
    this->declare_parameter<bool>("is_save_clusters", config.value("is_save_clusters", false));
    this->declare_parameter<float>("pca_threshold_multiplier", config.value("pca_threshold_multiplier", 2.5));
    return true;
}

/**
 * @brief 输出配置信息
 */
void RobotDetector::print_config() const {
    RCLCPP_INFO(this->get_logger(), "配置文件路径: %s", this->get_parameter("config_path").as_string().c_str());
    RCLCPP_INFO(this->get_logger(), "点云话题名称: %s", this->get_parameter("topic_name").as_string().c_str());
    RCLCPP_INFO(this->get_logger(), "特征文件夹路径: %s", this->get_parameter("feature_dir").as_string().c_str());
    RCLCPP_INFO(this->get_logger(), "最大点数限制: %ld", this->get_parameter("max_point_num").as_int());
    RCLCPP_INFO(this->get_logger(), "最小点数限制: %ld", this->get_parameter("min_point_num").as_int());
    RCLCPP_INFO(this->get_logger(), "特征匹配距离阈值: %f", this->get_parameter("templates_distance_threshold").as_double());
    RCLCPP_INFO(this->get_logger(), "体素尺寸: %f", this->get_parameter("voxel_size").as_double());
    RCLCPP_INFO(this->get_logger(), "统计滤波平均K值: %ld", this->get_parameter("statistical_mean_k").as_int());
    RCLCPP_INFO(this->get_logger(), "统计滤波标准差倍数阈值: %f", this->get_parameter("statistical_stddev_mul_thresh").as_double());
    RCLCPP_INFO(this->get_logger(), "聚类搜索半径: %f", this->get_parameter("radius_search").as_double());
    RCLCPP_INFO(this->get_logger(), "聚类曲率阈值: %f", this->get_parameter("curvature_threshold").as_double());
    RCLCPP_INFO(this->get_logger(), "聚类法线夹角阈值: %f", this->get_parameter("smoothness_threshold").as_double());
    RCLCPP_INFO(this->get_logger(), "聚类搜索邻域点数: %ld", this->get_parameter("number_of_neighbours").as_int());
    RCLCPP_INFO(this->get_logger(), "最小聚类大小: %ld", this->get_parameter("min_cluster_size").as_int());
    RCLCPP_INFO(this->get_logger(), "最大聚类大小: %ld", this->get_parameter("max_cluster_size").as_int());
    RCLCPP_INFO(this->get_logger(), "是否发布聚类结果: %s", this->get_parameter("is_pub_clusters").as_bool() ? "是" : "否");
    RCLCPP_INFO(this->get_logger(), "是否保存聚类结果: %s", this->get_parameter("is_save_clusters").as_bool() ? "是" : "否");
    RCLCPP_INFO(this->get_logger(), "PCA阈值倍数: %f", this->get_parameter("pca_threshold_multiplier").as_double());
}

/**
 * @brief 加载模板特征
 * @return 成功返回true
 */
bool RobotDetector::load_templates() {
    std::string feature_dir = this->get_parameter("feature_dir").as_string();
    std::string files_dir = feature_dir + "/avg_fpfh_features";
    if (!std::filesystem::exists(files_dir)) {
        RCLCPP_ERROR(this->get_logger(), "模板特征文件夹不存在: %s", files_dir.c_str());
        return false;
    }

    int loaded = 0;
    for (const auto& entry : std::filesystem::directory_iterator(files_dir)) {
        // 检查扩展名（注意加点）
        if (entry.path().extension() != ".fpfh") {
            RCLCPP_DEBUG(this->get_logger(), "跳过非 .fpfh 文件: %s", entry.path().c_str());
            continue;
        }

        std::ifstream ifs(entry.path().string(), std::ios::binary);
        if (!ifs) {
            RCLCPP_WARN(this->get_logger(), "无法打开文件: %s", entry.path().c_str());
            continue;
        }

        // 获取文件大小
        ifs.seekg(0, std::ios::end);
        size_t size = ifs.tellg();
        ifs.seekg(0, std::ios::beg);

        const size_t expected_size = 33 * sizeof(float);
        if (size != expected_size) {
            RCLCPP_WARN(this->get_logger(), "文件 %s 尺寸不匹配，期望 %zu 字节，实际 %zu 字节", entry.path().c_str(), expected_size, size);
            continue;
        }

        std::vector<float> data(33);
        ifs.read(reinterpret_cast<char*>(data.data()), size);
        if (!ifs) {
            RCLCPP_WARN(this->get_logger(), "读取文件失败: %s", entry.path().c_str());
            continue;
        }

        templates_.push_back(std::move(data));
        loaded++;
    }

    RCLCPP_INFO(this->get_logger(), "模板特征加载完成，共加载 %d 个模板", loaded);
    return loaded > 0;  // 可根据需要调整返回值条件
}

/**
 * @brief 加载点云数量范围
 * @return 成功返回true
 */
bool RobotDetector::load_points_number_ranges() {
    std::string feature_dir = this->get_parameter("feature_dir").as_string();
    std::string file_path = feature_dir + "/ranges.json";
    nlohmann::json points_number_ranges_;
    std::ifstream ifs(file_path);
    if (!ifs.is_open()) {
        RCLCPP_ERROR(this->get_logger(), "打开点云数量范围文件失败: %s， 使用默认配置", file_path.c_str());
        return false;
    }
    ifs >> points_number_ranges_;
    ifs.close();
    try {
        int max_point_num = points_number_ranges_["max_num_points"];
        int min_point_num = points_number_ranges_["min_num_points"];
        // 根据比例设置点的范围与聚类范围
        this->declare_parameter<int>("max_point_num", static_cast<int>(max_point_num * 1.5));
        this->declare_parameter<int>("min_point_num", static_cast<int>(min_point_num * 0.5));
        this->declare_parameter<int>("max_cluster_size", static_cast<int>(max_point_num * 1.5));
        this->declare_parameter<int>("min_cluster_size", static_cast<int>(min_point_num * 0.5));
        return true;
    } catch (const std::exception& e) {
        RCLCPP_ERROR(this->get_logger(), "解析点云数量范围文件失败: %s", file_path.c_str());
        return false;
    }
}

/**
 * @brief 计算平均FPFH特征
 * @param cloud 输入点云
 * @param feature 输出33维特征
 * @return 成功返回true
 */
bool RobotDetector::compute_avg_fpfh(const pcl::PointCloud<pcl::PointXYZ>::Ptr& cloud, std::vector<float>& feature) const {
    if (cloud->empty()) return false;

    // 计算法线
    pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> ne;
    pcl::PointCloud<pcl::Normal>::Ptr normals(new pcl::PointCloud<pcl::Normal>);
    pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>);
    tree->setInputCloud(cloud);
    ne.setInputCloud(cloud);
    ne.setSearchMethod(tree);
    ne.setKSearch(20);
    ne.compute(*normals);

    // 计算FPFH
    pcl::FPFHEstimation<pcl::PointXYZ, pcl::Normal, pcl::FPFHSignature33> fpfh;
    fpfh.setInputCloud(cloud);
    fpfh.setInputNormals(normals);
    fpfh.setSearchMethod(tree);
    fpfh.setKSearch(20);
    pcl::PointCloud<pcl::FPFHSignature33>::Ptr output(new pcl::PointCloud<pcl::FPFHSignature33>);
    fpfh.compute(*output);
    if (output->empty()) return false;

    // 计算平均直方图
    std::vector<float> sum(33, 0.0f);
    for (const auto& pt : output->points) {
        for (int i = 0; i < 33; ++i) {
            sum[i] += pt.histogram[i];
        }
    }
    size_t n = output->size();
    feature.resize(33);
    for (int i = 0; i < 33; ++i) {
        feature[i] = sum[i] / n;
    }
    return true;
}

/**
 * @brief 计算两个特征的欧氏距离
 */
float RobotDetector::compute_euclidean_distance(const std::vector<float>& a, const std::vector<float>& b) const {
    if (a.size() != b.size()) {
        return std::numeric_limits<float>::max();
    }
    float sq_sum = std::inner_product(a.begin(), a.end(), b.begin(), 0.0f, std::plus<>(), [](float x, float y) {
        float d = x - y;
        return d * d;
    });
    return std::sqrt(sq_sum);
}

/**
 * @brief 将距离转换为置信度 (0~1)
 * @param distance 欧氏距离
 * @return 置信度，距离越小置信度越高
 */
float RobotDetector::distance_to_confidence(float distance) const {
    const float threshold_start = 5.0f;
    const float threshold_mid = 9.0f;
    const float threshold_end = 25.0f;
    const float conf_mid = 0.8f;  // 距离=8 时的置信度

    if (distance <= threshold_start) return 1.0f;
    if (distance >= threshold_end) return 0.0f;

    if (distance <= threshold_mid) {
        // 区间 [5, 8]：线性从 1 降至 conf_mid
        float t = (distance - threshold_start) / (threshold_mid - threshold_start);
        return 1.0f - t * (1.0f - conf_mid);
    } else {
        // 区间 [8, 15]：线性从 conf_mid 降至 0
        float t = (distance - threshold_mid) / (threshold_end - threshold_mid);
        return conf_mid * (1.0f - t);
    }
}

/**
 * @brief 点云预处理（降采样、去噪、去地面）
 * @param cloud 输入点云
 * @return 处理后的点云
 */
pcl::PointCloud<pcl::PointXYZ>::Ptr RobotDetector::preprocess(const pcl::PointCloud<pcl::PointXYZ>::Ptr& cloud) const {
    float voxel_size = this->get_parameter("voxel_size").as_double();
    float statistical_mean_k = this->get_parameter("statistical_mean_k").as_int();
    float statistical_stddev_mul_thresh = this->get_parameter("statistical_stddev_mul_thresh").as_double();
    // 去除nan点
    pcl::PointCloud<pcl::PointXYZ>::Ptr rm_nan_cloud(new pcl::PointCloud<pcl::PointXYZ>);
    std::vector<int> indices;
    pcl::removeNaNFromPointCloud(*cloud, *rm_nan_cloud, indices);

    // 体素滤波
    pcl::VoxelGrid<pcl::PointXYZ> voxel_grid;
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_downsampled(new pcl::PointCloud<pcl::PointXYZ>);
    voxel_grid.setInputCloud(rm_nan_cloud);
    voxel_grid.setLeafSize(voxel_size, voxel_size, voxel_size);
    voxel_grid.filter(*cloud_downsampled);

    // 统计滤波去噪
    pcl::StatisticalOutlierRemoval<pcl::PointXYZ> sor;
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_filtered(new pcl::PointCloud<pcl::PointXYZ>);
    sor.setInputCloud(cloud_downsampled);
    sor.setMeanK(statistical_mean_k);
    sor.setStddevMulThresh(statistical_stddev_mul_thresh);
    sor.filter(*cloud_filtered);

    return cloud_filtered;
}

std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr> RobotDetector::region_growing(const pcl::PointCloud<pcl::PointXYZ>::Ptr& cloud) const {
    std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr> clusters;
    int min_cluster_size = this->get_parameter("min_cluster_size").as_int();
    int max_cluster_size = this->get_parameter("max_cluster_size").as_int();
    float radius_search = this->get_parameter("radius_search").as_double();
    int number_of_neighbours = this->get_parameter("number_of_neighbours").as_int();
    float curvature_threshold = this->get_parameter("curvature_threshold").as_double();
    float smoothness_threshold = this->get_parameter("smoothness_threshold").as_double();

    if (!cloud || cloud->empty()) {
        return clusters;
    }

    // 1. 计算法线和曲率
    pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> ne;
    ne.setInputCloud(cloud);
    pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>);
    ne.setSearchMethod(tree);
    pcl::PointCloud<pcl::Normal>::Ptr normals(new pcl::PointCloud<pcl::Normal>);
    ne.setRadiusSearch(radius_search);  // 法线计算半径（米），可根据点云密度调整
    ne.compute(*normals);

    // 2. 创建区域生长对象
    pcl::RegionGrowing<pcl::PointXYZ, pcl::Normal> reg;
    reg.setInputCloud(cloud);
    reg.setInputNormals(normals);

    // 设置区域生长参数
    reg.setMinClusterSize(min_cluster_size);                           // 最小簇大小（点数），过滤噪声
    reg.setMaxClusterSize(max_cluster_size);                           // 最大簇大小
    reg.setNumberOfNeighbours(number_of_neighbours);                   // 搜索邻居数（影响算法速度与精度）
    reg.setCurvatureThreshold(curvature_threshold);                    // 曲率阈值，平滑区域合并（可调）
    reg.setSmoothnessThreshold(smoothness_threshold / 180.0f * M_PI);  // 法线夹角阈值（弧度），60度

    // 3. 执行分割，获取簇索引
    std::vector<pcl::PointIndices> cluster_indices;
    reg.extract(cluster_indices);

    // 4. 根据索引提取点云
    for (const auto& indices : cluster_indices) {
        pcl::PointCloud<pcl::PointXYZ>::Ptr cluster(new pcl::PointCloud<pcl::PointXYZ>);
        cluster->reserve(indices.indices.size());
        for (int idx : indices.indices) {
            cluster->push_back(cloud->points[idx]);
        }
        cluster->width = static_cast<uint32_t>(cluster->size());
        cluster->height = 1;
        cluster->is_dense = true;
        clusters.push_back(cluster);
    }

    return clusters;
}
/**
 * @brief 计算质心
 * @param cluster 聚类簇
 * @return 质心坐标
 */
Eigen::Vector4f RobotDetector::compute_centroid(const pcl::PointCloud<pcl::PointXYZ>::Ptr& cluster) const {
    Eigen::Vector4f centroid;
    pcl::compute3DCentroid(*cluster, centroid);
    return centroid;
}

void RobotDetector::save_clusters(const std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr>& clusters, const std::string& root_folder) {
    // 1. 生成时间戳字符串 (格式: YYYY_MMDD_HHMM)
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now;
    localtime_r(&time_t_now, &tm_now);  // 线程安全，Windows 可用 localtime_s

    std::ostringstream timestamp_ss;
    timestamp_ss << std::put_time(&tm_now, "%Y_%m%d_%H%M");
    std::string timestamp = timestamp_ss.str();

    // 2. 构建完整保存路径
    std::string save_dir = root_folder + "/" + timestamp;
    std::filesystem::path dir_path(save_dir);

    // 3. 创建目录（如果不存在）
    if (!std::filesystem::exists(dir_path)) {
        if (!std::filesystem::create_directories(dir_path)) {
            std::cerr << "Failed to create directory: " << save_dir << std::endl;
            return;
        }
    }

    // 4. 遍历并保存每个簇
    for (size_t i = 0; i < clusters.size(); ++i) {
        const auto& cloud = clusters[i];
        if (!cloud || cloud->empty()) {
            std::cout << "Cluster " << i << " is empty, skipping." << std::endl;
            continue;
        }

        // 生成文件名
        std::ostringstream filename_ss;
        filename_ss << "cluster_" << i << ".pcd";
        std::string file_path = save_dir + "/" + filename_ss.str();

        // 保存为二进制格式（效率高）
        if (pcl::io::savePCDFileBinary(file_path, *cloud) == 0) {
            RCLCPP_DEBUG(this->get_logger(), "保存 聚类结果 %ld to %s", i, file_path.c_str());
        } else {
            RCLCPP_ERROR(this->get_logger(), "保存 聚类结果 %ld to %s 失败", i, file_path.c_str());
        }
    }
}

sensor_msgs::msg::PointCloud2 RobotDetector::convertClustersToColoredPointCloud(
    const std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr>& clusters, const std::string& frame_id) {
    // 存储所有带颜色的点
    pcl::PointCloud<pcl::PointXYZRGB> colored_cloud;

    // 1. 为每个簇生成一个随机颜色（或使用固定颜色列表）
    std::vector<std::vector<uint8_t>> cluster_colors;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    for (size_t i = 0; i < clusters.size(); ++i) {
        // 随机生成 RGB 颜色（也可根据索引循环使用固定色板）
        uint8_t r = static_cast<uint8_t>(dis(gen));
        uint8_t g = static_cast<uint8_t>(dis(gen));
        uint8_t b = static_cast<uint8_t>(dis(gen));
        cluster_colors.push_back({r, g, b});
    }

    // 2. 遍历每个簇，为每个点赋予颜色
    for (size_t i = 0; i < clusters.size(); ++i) {
        const auto& cloud = clusters[i];
        if (!cloud || cloud->empty()) continue;

        auto& color = cluster_colors[i];
        uint8_t r = color[0];
        uint8_t g = color[1];
        uint8_t b = color[2];

        for (const auto& pt : cloud->points) {
            pcl::PointXYZRGB rgb_pt;
            rgb_pt.x = pt.x;
            rgb_pt.y = pt.y;
            rgb_pt.z = pt.z;
            rgb_pt.r = r;
            rgb_pt.g = g;
            rgb_pt.b = b;
            colored_cloud.push_back(rgb_pt);
        }
    }

    // 3. 设置点云参数
    colored_cloud.width = static_cast<uint32_t>(colored_cloud.size());
    colored_cloud.height = 1;
    colored_cloud.is_dense = true;

    // 4. 转换为 ROS2 消息
    sensor_msgs::msg::PointCloud2 output;
    pcl::toROSMsg(colored_cloud, output);
    // 设置坐标系（通常与原始点云一致，这里需要从外部传入，或者用固定值）
    output.header.frame_id = frame_id;            // 或根据需要设置
    output.header.stamp = rclcpp::Clock().now();  // 或从外部传入时间戳
    return output;
}

/**
 * @brief 使用 PCA（马氏距离）剔除点云中的线状干扰。
 * @param cloud 输入点云簇（包含球体+线）
 * @param threshold_multiplier 阈值倍数（均值 + 倍数 * 标准差），建议 2.0~3.0
 * @return 去除线后的点云
 */
pcl::PointCloud<pcl::PointXYZ>::Ptr RobotDetector::remove_lines_using_pca(const pcl::PointCloud<pcl::PointXYZ>::Ptr& cloud,
                                                                          double threshold_multiplier) {
    pcl::PointCloud<pcl::PointXYZ>::Ptr result(new pcl::PointCloud<pcl::PointXYZ>);
    if (cloud && cloud->empty()) return result;

    // 1. 计算主成分
    pcl::PCA<pcl::PointXYZ> pca;
    pca.setInputCloud(cloud);
    Eigen::Vector4f centroid = pca.getMean();               // 质心
    Eigen::Matrix3f eigen_vectors = pca.getEigenVectors();  // 主方向
    Eigen::Vector3f eigen_values = pca.getEigenValues();    // 特征值（方差）

    // 2. 计算每个点的马氏距离平方
    std::vector<double> mahal_dist_sq(cloud->size());
    for (size_t i = 0; i < cloud->size(); ++i) {
        Eigen::Vector3f pt(cloud->points[i].x, cloud->points[i].y, cloud->points[i].z);
        Eigen::Vector3f centered = pt - centroid.head<3>();

        // 投影到主成分空间
        Eigen::Vector3f transformed;
        transformed.x() = centered.dot(eigen_vectors.col(0));
        transformed.y() = centered.dot(eigen_vectors.col(1));
        transformed.z() = centered.dot(eigen_vectors.col(2));

        // 计算马氏距离平方（避免除零）
        double dist_sq = 0.0;
        for (int j = 0; j < 3; ++j) {
            if (eigen_values(j) > 1e-6) {
                dist_sq += (transformed(j) * transformed(j)) / eigen_values(j);
            }
        }
        mahal_dist_sq[i] = dist_sq;
    }

    // 3. 确定阈值（基于均值 + 倍数*标准差）
    double sum = 0.0, sum_sq = 0.0;
    for (double d : mahal_dist_sq) {
        sum += d;
        sum_sq += d * d;
    }
    double mean = sum / mahal_dist_sq.size();
    double variance = sum_sq / mahal_dist_sq.size() - mean * mean;
    double stddev = std::sqrt(std::max(0.0, variance));
    double threshold = mean + threshold_multiplier * stddev;

    // 可选：使用百分位数（如95%）更鲁棒
    /*
    std::vector<double> sorted = mahal_dist_sq;
    std::sort(sorted.begin(), sorted.end());
    double threshold = sorted[static_cast<size_t>(0.95 * sorted.size())];
    */

    // 4. 提取内点（马氏距离小于阈值）
    for (size_t i = 0; i < cloud->size(); ++i) {
        if (mahal_dist_sq[i] <= threshold) {
            result->push_back(cloud->points[i]);
        }
    }

    return result;
}
