#pragma once

#include <rclcpp/rclcpp.hpp>
#include <systemd/sd-daemon.h>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <unordered_set>
#include <sstream>

// 简单的字符串连接函数
inline std::string join_strings(const std::vector<std::string>& vec, const std::string& delim = ", ") {
  std::ostringstream oss;
  for (size_t i = 0; i < vec.size(); ++i) {
    oss << vec[i];
    if (i != vec.size() - 1) {
      oss << delim;
    }
  }
  return oss.str();
}

inline void wait_for_services_registration(
  const std::shared_ptr<rclcpp::Node>& node,
  const std::vector<std::string>& service_base_names,
  int max_retry = 100,
  int rate_hz = 10)
{
  auto context = node->get_node_base_interface()->get_context();
  rclcpp::Rate rate(rate_hz);
  int retry = 0;

  std::string ns = node->get_namespace();
  if (ns.empty() || ns == "/") {
    ns = "";
  }

  // 要等待的服务集合
  std::unordered_set<std::string> expected_services;
  for (const auto& name : service_base_names) {
    expected_services.insert(ns + "/" + name);
  }

  RCLCPP_INFO(node->get_logger(), "等待 %zu 个服务注册中...", expected_services.size());

  while (context->is_valid()) {
    std::unordered_set<std::string> registered;
    auto services = node->get_service_names_and_types_by_node(
      node->get_name(), node->get_namespace());

    for (const auto& entry : services) {
      registered.insert(entry.first);
      RCLCPP_INFO(node->get_logger(),"已注册服务: %s", entry.first.c_str());
    }

    std::vector<std::string> missing;
    for (const auto& expected : expected_services) {
      if (registered.find(expected) == registered.end()) {
        missing.push_back(expected);
      }
    }

    if (missing.empty()) {
      RCLCPP_INFO(node->get_logger(), "所有服务已注册，准备发送 READY=1");
      std::this_thread::sleep_for(std::chrono::seconds(10));
      sd_notify(0, "READY=1");
      return;
    }

    RCLCPP_INFO(node->get_logger(), "尚未注册的服务: [%s]", join_strings(missing).c_str());

    if (++retry >= max_retry) {
      RCLCPP_ERROR(node->get_logger(), "等待服务注册超时，未注册的服务有: [%s]", join_strings(missing).c_str());
      return;
    }

    rate.sleep();
  }

  RCLCPP_ERROR(node->get_logger(), "上下文无效，停止等待服务注册");
}
