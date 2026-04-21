/*
 * @Author: TianqiTang
 * @Date: 2025-08-27 13:10:46
 * @LastEditors: TianqiTang
 * @LastEditTime: 2025-08-28 14:16:17
 * @FilePath: /Photonix/src/common_utils/include/common_utils/config.hpp
 */


#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "fileExt.hpp"

enum LidarType {
  ZWSCAN360 = 2, // 暂时为伊犁那款雷达
  ZWSCAN80 = 1,
  ZWSCAN160 = 3, // R1
};

inline LidarType getLidarType() {
  wait_for_Config("/opt/zwkj/configs/lidar_config.json");
  auto configs = readFromFile("/opt/zwkj/configs/lidar_config.json");
  
  // 遍历所有雷达配置
  for (const auto& lidar : configs["lidars"].items()) {
      const std::string& sn = lidar.value()["sn"];
      
      // 查找最后一个下划线位置
      size_t pos = sn.rfind('_');
      if (pos != std::string::npos) {
          std::string type = sn.substr(pos + 1);
          if (type == "ZWSCAN360") {
              return ZWSCAN360;
          } else if (type == "ZWSCAN160") {
              return ZWSCAN160;
          }
      }
  }
  
  // 默认返回 ZWSCAN80
  return ZWSCAN80;
}

#endif // CONFIG_HPP