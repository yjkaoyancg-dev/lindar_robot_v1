/*******************************************************************************
****FilePath: /Photonix/src/common_utils/include/common_utils/sync_helper.hpp
****Author: mwt 911608720@qq.com
****Date: 2025-05-23 14:14:08
****Description: 
****Copyright: 2025 by mwt, All Rights Reserved.
********************************************************************************/
#pragma once
#include <nlohmann/json.hpp>
/*
    @param seq: 输入的seq格式: 原始seq-模块名称-index
    @return: 原始seq
*/
#include <string>

inline std::string getBaseSeq(const std::string &seq)
{
    std::string::size_type pos = seq.find_first_of('-');
    if (pos != std::string::npos)
    {
        return seq.substr(0, pos);
    }
    return seq;
}
inline std::string getSEQ(){
    //TODO 根据不同的要求实现SEQ
    return "SEQ";
}
template<typename... JsonTasks>
inline nlohmann::json build_up_task(const std::string &seq,
                                    bool strategy = true,
                                    int timeout = 300,
                                    const JsonTasks&... tasks_) {
    nlohmann::json task;
    task["seq"] = seq;
    task["strategy"] = strategy ? "parallel" : "serial";
    task["timeout"] = timeout;
    task["tasks"] = nlohmann::json::array();
    (task["tasks"].push_back(tasks_), ...);
    return task;
}

