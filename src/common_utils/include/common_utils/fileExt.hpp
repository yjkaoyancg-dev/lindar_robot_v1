/*******************************************************************************
****FilePath: /Photonix/src/common_utils/include/common_utils/fileExt.hpp
****Author: mwt 911608720@qq.com
****Date: 2025-04-08 12:55:13
****Description: 文件操作相关的函数
****Copyright: 2025 by mwt, All Rights Reserved.
********************************************************************************/
#pragma once
#include <sys/inotify.h>
#include <unistd.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <thread>

inline bool fileExists(const std::string &filePath) {
  return std::filesystem::exists(filePath);
}

inline void wait_for_Config(const std::string &filePath) {
  std::cout << "等待配置文件: " << filePath << std::endl;
  // 首先检查文件是否已经存在
  if (access(filePath.c_str(), F_OK) == 0) {
    // 文件已存在，直接返回
    std::cout << "文件已存在: " << filePath << std::endl;
    return;
  }
  // 提取文件所在的目录和目标文件名
  size_t pos = filePath.find_last_of('/');
  std::string directory =
      (pos == std::string::npos) ? "." : filePath.substr(0, pos);
  std::string fileName = filePath.substr(pos + 1);
  // 初始化 inotify 阻塞模式
  int fd = inotify_init1(0);
  if (fd < 0) {
    std::cerr << "Failed to initialize inotify: " << strerror(errno)
              << std::endl;
    return;
  }
  // 添加对目录的监听，监听文件创建事件
  int wd = inotify_add_watch(fd, directory.c_str(), IN_CREATE);
  if (wd < 0) {
    std::cerr << "Failed to add watch: " << strerror(errno) << std::endl;
    close(fd);
    return;
  }
  char buffer[1024];
  while (true) {
    ssize_t length = read(fd, buffer, sizeof(buffer));
    if (length < 0) {
      if (errno == EINTR) continue;  // 被信号中断，继续读取
      std::cerr << "Read error: " << strerror(errno) << std::endl;
      break;
    }

    ssize_t i = 0;
    while (i < length) {
      struct inotify_event *event = (struct inotify_event *)&buffer[i];
      // 检查事件类型是否为文件创建
      if (event->mask & IN_CREATE) {
        std::string eventFileName(event->name);
        // 比较事件中的文件名和目标文件名
        if (eventFileName == fileName) {
          // 目标文件已创建，退出函数
          inotify_rm_watch(fd, wd);
          close(fd);
          return;
        }
      }
      i += sizeof(struct inotify_event) + event->len;
    }
  }
  // 清理资源
  inotify_rm_watch(fd, wd);
  close(fd);
  std::cout << "文件已创建: " << filePath << std::endl;
}

inline void listen_directory_change(
    const std::string &dirPath, const std::string &targetFile,
    std::function<void(const std::string &)> on_file_changed,
    const std::string &optional_prefix = "",
    const std::string &optional_suffix = "", int debounce_ms = 100) {
  std::thread([=]() {
    int fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0) {
      std::cerr << "Failed to init inotify: " << strerror(errno) << std::endl;
      return;
    }

    int wd = inotify_add_watch(fd, dirPath.c_str(),
                               IN_MODIFY | IN_CLOSE_WRITE | IN_MOVED_TO);
    if (wd < 0) {
      std::cerr << "Failed to add watch on dir: " << strerror(errno)
                << std::endl;
      close(fd);
      return;
    }

    std::map<std::string, std::chrono::steady_clock::time_point>
        last_trigger_time;
    char buffer[4096]
        __attribute__((aligned(__alignof__(struct inotify_event))));

    while (true) {
      ssize_t length = read(fd, buffer, sizeof(buffer));
      if (length < 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }

      ssize_t i = 0;
      while (i < length) {
        struct inotify_event *event = (struct inotify_event *)&buffer[i];
        if (event->len > 0) {
          std::string fname(event->name);

          bool match = false;
          if (!targetFile.empty()) {
            match = (fname == targetFile);
          } else {
            if (!optional_prefix.empty() &&
                fname.rfind(optional_prefix, 0) != 0)
              match = false;
            else if (!optional_suffix.empty() &&
                     (fname.size() < optional_suffix.size() ||
                      fname.compare(fname.size() - optional_suffix.size(),
                                    optional_suffix.size(),
                                    optional_suffix) != 0))
              match = false;
            else
              match = true;
          }

          if (match &&
              (event->mask & (IN_MODIFY | IN_CLOSE_WRITE | IN_MOVED_TO))) {
            auto now = std::chrono::steady_clock::now();
            auto &last_time = last_trigger_time[fname];
            if (now - last_time > std::chrono::milliseconds(debounce_ms)) {
              on_file_changed(fname);
              last_time = now;
            }
          }
        }
        i += sizeof(struct inotify_event) + event->len;
      }
    }

    inotify_rm_watch(fd, wd);
    close(fd);
  }).detach();
}

inline nlohmann::json readFromFile(const std::string &filePath) {
  std::ifstream stream(filePath);
  if (!stream.is_open()) {
    throw std::runtime_error("无法打开文件: " + filePath);
  }
  std::stringstream buffer;
  buffer << stream.rdbuf();
  return nlohmann::json::parse(buffer.str());
}
inline std::string getUUID() {
  static std::string uuid;
  if (uuid.empty()) {
    wait_for_Config("/opt/zwkj/configs/muid.json");
    auto uuid_json = readFromFile("/opt/zwkj/configs/muid.json");
    // wait_for_Config("/home/chx/ZWKJ/mwt/Photonix/configs/muid.json");
    // auto uuid_json = readFromFile("/home/chx/ZWKJ/mwt/Photonix/configs/muid.json");
    uuid = uuid_json["muid"];
  }
  return uuid;
}