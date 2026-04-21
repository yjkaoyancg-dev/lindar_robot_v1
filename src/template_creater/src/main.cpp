#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <filesystem>
#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl/features/normal_3d.h>
#include <pcl/features/fpfh.h>
#include <pcl/filters/filter.h>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

// 计算平均FPFH特征（33维），输入点云已去除NaN点
bool computeAvgFPFH(const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& cloud,
                    std::vector<float>& feature) {
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

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cout << "用法: " << argv[0] << " <输入文件夹路径> <输出文件夹路径>" << std::endl;
        return -1;
    }

    fs::path input_dir(argv[1]);
    fs::path output_dir(argv[2]);

    if (!fs::exists(input_dir) || !fs::is_directory(input_dir)) {
        std::cerr << "输入文件夹不存在或不是目录" << std::endl;
        return -1;
    }

    // 创建输出文件夹（用于存放FPFH特征）
    fs::create_directories(output_dir);
    fs::path fpfh_dir = output_dir / "avg_fpfh_features";
    fs::create_directories(fpfh_dir);

    // 存储所有样本的点数，用于计算范围
    std::vector<float> all_num_points;

    // 遍历输入文件夹中的所有.pcd文件
    for (const auto& entry : fs::directory_iterator(input_dir)) {
        if (entry.path().extension() != ".pcd") continue;

        std::string filename = entry.path().stem().string();
        std::cout << "处理文件: " << entry.path() << std::endl;

        // 读取点云
        pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>);
        if (pcl::io::loadPCDFile<pcl::PointXYZ>(entry.path().string(), *cloud) == -1) {
            std::cerr << "无法读取文件: " << entry.path() << std::endl;
            continue;
        }

        // 去除NaN点
        pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_clean(new pcl::PointCloud<pcl::PointXYZ>);
        std::vector<int> indices;
        pcl::removeNaNFromPointCloud(*cloud, *cloud_clean, indices);
        if (cloud_clean->empty()) {
            std::cerr << "点云为空或全部为NaN: " << filename << std::endl;
            continue;
        }

        // 记录点数
        size_t n = cloud_clean->size();
        all_num_points.push_back(static_cast<float>(n));

        // ----- 平均FPFH特征 -----
        std::vector<float> fpfh;
        if (computeAvgFPFH(cloud_clean, fpfh)) {
            fs::path out_file = fpfh_dir / (filename + ".fpfh");
            std::ofstream ofs(out_file.string(), std::ios::binary);
            ofs.write(reinterpret_cast<const char*>(fpfh.data()), fpfh.size() * sizeof(float));
            ofs.close();
        } else {
            std::cerr << "平均FPFH计算失败: " << filename << std::endl;
        }
    }

    if (all_num_points.empty()) {
        std::cout << "没有找到有效的PCD文件" << std::endl;
        return 0;
    }

    // 计算最小和最大点数
    float min_pts = *std::min_element(all_num_points.begin(), all_num_points.end());
    float max_pts = *std::max_element(all_num_points.begin(), all_num_points.end());

    // 构建JSON，仅包含点数范围
    json j;
    j["min_num_points"] = min_pts;
    j["max_num_points"] = max_pts;

    fs::path json_file = output_dir / "ranges.json";
    std::ofstream ofs(json_file.string());
    ofs << j.dump(4);
    ofs.close();

    std::cout << "处理完成，结果保存在: " << output_dir << std::endl;
    return 0;
}