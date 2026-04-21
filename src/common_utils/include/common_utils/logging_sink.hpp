#pragma once

#include <string>

namespace common_utils {

void InstallProcessWideLogSink();

std::string ResolveLogBaseDirectory();
std::string NormalizeLoggerNameForFile(const std::string &logger_name);

}  // namespace common_utils
