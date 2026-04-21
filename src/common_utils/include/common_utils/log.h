/*******************************************************************************
****FilePath: /Photonix/src/common_utils/include/common_utils/log.h
****Author: mwt 911608720@qq.com
****Date: 2025-04-27 14:55:36
****Description: 日志宏
****Copyright: 2025 by mwt, All Rights Reserved.
********************************************************************************/
#pragma once
#include <string>
#include <std_msgs/msg/string.hpp>

template<class PubPtr>
inline void zlog(PubPtr pub, const std::string& text)
{
  std_msgs::msg::String msg;
  msg.data = text;
  pub->publish(msg);
}