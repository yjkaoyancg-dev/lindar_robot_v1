/*******************************************************************************
****FilePath: /Photonix/src/common_utils/include/common_utils/time.hpp
****Author: mwt 911608720@qq.com
****Date: 2025-04-14 13:43:58
****Description: 
****Copyright: 2025 by mwt, All Rights Reserved.
********************************************************************************/
#pragma once
#include <ctime>
#include <string>
#include <cstring>
#include <chrono>
#include <sstream>
#include <iomanip>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

inline std::string getCurrentDateTimeString(bool with_subseconds) {
    using namespace std::chrono;

    // 获取当前系统时间
    auto now = system_clock::now();

    // 拆分为 time_t 和本地时间
    std::time_t now_time_t = system_clock::to_time_t(now);
    std::tm now_tm = *std::localtime(&now_time_t);

    std::ostringstream oss;
    oss << std::put_time(&now_tm, "%Y%m%d%H%M%S");

    if (with_subseconds) {
        // 计算从 epoch 到 now 的纳秒数
        auto ns_since_epoch = duration_cast<nanoseconds>(now.time_since_epoch());
        auto ns_part = ns_since_epoch % seconds(1);  // 这一秒中的纳秒数

        int milliseconds = static_cast<int>(ns_part.count() / 1'000'000);       // 毫秒
        int nanosec_remainder = static_cast<int>((ns_part.count() % 1'000'000) / 1'000);  // 纳秒后三位

        oss << std::setw(3) << std::setfill('0') << milliseconds
            << std::setw(3) << std::setfill('0') << nanosec_remainder;
    }

    return oss.str();
}

inline double secondsToFrequency(double interval_seconds) {
    if (interval_seconds <= 0.0) {
        return 1.0;  // 默认最低频率为 1Hz
    }
    return 1.0 / interval_seconds;
}