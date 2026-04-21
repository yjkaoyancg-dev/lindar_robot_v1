#include "common_utils/logging_sink.hpp"

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <rcutils/logging.h>
#include <rcutils/types/char_array.h>
#include <rclcpp/rclcpp.hpp>

namespace common_utils {
namespace {

struct FileHandle {
  std::string date_key;
  std::string file_path;
  std::unique_ptr<std::ofstream> stream;
};

bool IsAllDigits(const std::string &token) {
  if (token.empty()) {
    return false;
  }
  for (char ch : token) {
    if (!std::isdigit(static_cast<unsigned char>(ch))) {
      return false;
    }
  }
  return true;
}

bool IsUpperAlphaNum(const std::string &token) {
  if (token.empty()) {
    return false;
  }
  bool has_alpha = false;
  for (char ch : token) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    if (!std::isalnum(uch)) {
      return false;
    }
    if (std::isalpha(uch)) {
      has_alpha = true;
    }
    if (std::isalpha(uch) && !std::isupper(uch)) {
      return false;
    }
  }
  return has_alpha;
}

bool IsTimestampLike(const std::string &token) {
  return token.size() >= 8 && token.size() <= 17 && IsAllDigits(token);
}

bool IsLikelyDynamicToken(const std::string &token) {
  if (token.size() < 6) {
    return false;
  }

  bool has_digit = false;
  bool has_alpha = false;
  bool has_lower = false;
  for (char ch : token) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    if (!std::isalnum(uch)) {
      return false;
    }
    has_digit = has_digit || std::isdigit(uch);
    has_alpha = has_alpha || std::isalpha(uch);
    has_lower = has_lower || std::islower(uch);
  }

  if (has_digit) {
    return true;
  }

  return has_alpha && !has_lower && IsUpperAlphaNum(token);
}

std::vector<std::string> Split(const std::string &value, char delimiter) {
  std::vector<std::string> parts;
  std::stringstream stream(value);
  std::string part;
  while (std::getline(stream, part, delimiter)) {
    if (!part.empty()) {
      parts.push_back(part);
    }
  }
  return parts;
}

std::string Join(const std::vector<std::string> &parts, char delimiter) {
  if (parts.empty()) {
    return "default";
  }

  std::ostringstream oss;
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i != 0) {
      oss << delimiter;
    }
    oss << parts[i];
  }
  return oss.str();
}

std::string SanitizePathSegment(std::string value) {
  if (value.empty()) {
    return "default";
  }

  for (char &ch : value) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    if (!(std::isalnum(uch) || ch == '_' || ch == '-' || ch == '.')) {
      ch = '_';
    }
  }
  return value;
}

std::string ExtractBasename(const std::string &logger_name) {
  const size_t pos = logger_name.find_last_of('/');
  if (pos == std::string::npos) {
    return logger_name;
  }
  return logger_name.substr(pos + 1);
}

bool EnsureDirectory(const std::string &path) {
  if (path.empty()) {
    return false;
  }

  if (path == "/") {
    return true;
  }

  std::string current;
  if (path.front() == '/') {
    current = "/";
  }

  std::stringstream stream(path);
  std::string segment;
  while (std::getline(stream, segment, '/')) {
    if (segment.empty()) {
      continue;
    }

    if (!current.empty() && current.back() != '/') {
      current += "/";
    }
    current += segment;

    if (::mkdir(current.c_str(), 0755) != 0 && errno != EEXIST) {
      return false;
    }
  }
  return true;
}

std::string GetEnvValue(const char *name) {
  const char *value = std::getenv(name);
  if (value == nullptr) {
    return "";
  }
  return value;
}

class LogFileManager {
 public:
  static LogFileManager &Instance() {
    static LogFileManager instance;
    return instance;
  }

  void Install() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (installed_) {
      return;
    }

    original_handler_ = rcutils_logging_get_output_handler();
    rcutils_logging_set_output_handler(&LogFileManager::OutputHandler);
    installed_ = true;
  }

  static void OutputHandler(const rcutils_log_location_t *location, int severity,
                            const char *name,
                            rcutils_time_point_value_t timestamp,
                            const char *format, va_list *args) {
    LogFileManager &manager = Instance();
    manager.ForwardToConsole(location, severity, name, timestamp, format, args);
    manager.WriteToFile(location, severity, name, timestamp, format, args);
  }

 private:
  LogFileManager() : base_dir_(ResolveLogBaseDirectory()) {}

  void ForwardToConsole(const rcutils_log_location_t *location, int severity,
                        const char *name, rcutils_time_point_value_t timestamp,
                        const char *format, va_list *args) {
    if (original_handler_ == nullptr) {
      return;
    }

    va_list console_args;
    va_copy(console_args, *args);
    original_handler_(location, severity, name, timestamp, format, &console_args);
    va_end(console_args);
  }

  static std::string FormatDate(rcutils_time_point_value_t timestamp) {
    const auto time_point = std::chrono::system_clock::time_point(
        std::chrono::nanoseconds(timestamp));
    const std::time_t time_value =
        std::chrono::system_clock::to_time_t(time_point);
    std::tm local_tm;
    localtime_r(&time_value, &local_tm);

    char buffer[16];
    std::strftime(buffer, sizeof(buffer), "%Y%m%d", &local_tm);
    return buffer;
  }

  static std::string FormatTimestamp(rcutils_time_point_value_t timestamp) {
    const auto time_point = std::chrono::system_clock::time_point(
        std::chrono::nanoseconds(timestamp));
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
                             time_point.time_since_epoch())
                             .count() %
                         1000;
    const std::time_t time_value =
        std::chrono::system_clock::to_time_t(time_point);
    std::tm local_tm;
    localtime_r(&time_value, &local_tm);

    char time_buffer[32];
    std::strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S",
                  &local_tm);

    std::ostringstream oss;
    oss << time_buffer << "." << std::setfill('0') << std::setw(3) << millis;
    return oss.str();
  }

  static const char *SeverityToString(int severity) {
    switch (severity) {
      case RCUTILS_LOG_SEVERITY_DEBUG:
        return "DEBUG";
      case RCUTILS_LOG_SEVERITY_INFO:
        return "INFO";
      case RCUTILS_LOG_SEVERITY_WARN:
        return "WARN";
      case RCUTILS_LOG_SEVERITY_ERROR:
        return "ERROR";
      case RCUTILS_LOG_SEVERITY_FATAL:
        return "FATAL";
      default:
        return "UNSET";
    }
  }

  static std::string FormatMessage(const char *format, va_list *args) {
    rcutils_allocator_t allocator = rcutils_get_default_allocator();
    rcutils_char_array_t output = rcutils_get_zero_initialized_char_array();
    if (rcutils_char_array_init(&output, 0, &allocator) != RCUTILS_RET_OK) {
      rcutils_reset_error();
      return "";
    }

    va_list file_args;
    va_copy(file_args, *args);
    const rcutils_ret_t ret =
        rcutils_char_array_vsprintf(&output, format, file_args);
    va_end(file_args);

    std::string message;
    if (ret == RCUTILS_RET_OK && output.buffer != nullptr) {
      message = output.buffer;
    } else {
      rcutils_reset_error();
    }

    const rcutils_ret_t fini_ret = rcutils_char_array_fini(&output);
    (void)fini_ret;
    return message;
  }

  bool EnsureStreamFor(const std::string &logger_name,
                       rcutils_time_point_value_t timestamp,
                       FileHandle *&handle_out) {
    const std::string date_key = FormatDate(timestamp);
    FileHandle &handle = files_[logger_name];
    if (handle.stream && handle.date_key == date_key && handle.stream->is_open()) {
      handle_out = &handle;
      return true;
    }

    const std::string directory = base_dir_ + "/" + logger_name;
    if (!EnsureDirectory(directory)) {
      return false;
    }

    const std::string file_path = directory + "/" + date_key + ".log";
    std::unique_ptr<std::ofstream> stream(
        new std::ofstream(file_path.c_str(), std::ios::out | std::ios::app));
    if (!stream->is_open()) {
      return false;
    }

    handle.date_key = date_key;
    handle.file_path = file_path;
    handle.stream = std::move(stream);
    handle_out = &handle;
    return true;
  }

  void WriteToFile(const rcutils_log_location_t *location, int severity,
                   const char *name, rcutils_time_point_value_t timestamp,
                   const char *format, va_list *args) {
    (void)location;

    const std::string normalized_logger =
        SanitizePathSegment(NormalizeLoggerNameForFile(name == nullptr ? "" : name));
    const std::string message = FormatMessage(format, args);
    if (message.empty()) {
      return;
    }

    const std::string line = "[" + FormatTimestamp(timestamp) + "] [" +
                             SeverityToString(severity) + "] [" +
                             normalized_logger + "] " + message + "\n";

    std::lock_guard<std::mutex> lock(mutex_);
    FileHandle *handle = nullptr;
    if (!EnsureStreamFor(normalized_logger, timestamp, handle) || handle == nullptr ||
        handle->stream == nullptr) {
      return;
    }

    *(handle->stream) << line;
    handle->stream->flush();
  }

  std::mutex mutex_;
  std::map<std::string, FileHandle> files_;
  rcutils_logging_output_handler_t original_handler_{nullptr};
  std::string base_dir_;
  bool installed_{false};
};

std::string GetDefaultLogBaseDirectory() {
  const std::vector<std::string> candidates = {
      "/opt/zwkj/logs",
      GetEnvValue("HOME").empty()
          ? std::string()
          : GetEnvValue("HOME") + "/.ros/photonix_logs",
      "/tmp/photonix_logs"};

  for (const std::string &candidate : candidates) {
    if (!candidate.empty() && EnsureDirectory(candidate)) {
      return candidate;
    }
  }
  return "/tmp/photonix_logs";
}

}  // namespace

std::string ResolveLogBaseDirectory() {
  const std::string env_dir = GetEnvValue("PHOTONIX_LOG_DIR");
  if (!env_dir.empty() && EnsureDirectory(env_dir)) {
    return env_dir;
  }

  const std::string legacy_env_dir = GetEnvValue("ZWKJ_LOG_DIR");
  if (!legacy_env_dir.empty() && EnsureDirectory(legacy_env_dir)) {
    return legacy_env_dir;
  }

  return GetDefaultLogBaseDirectory();
}

std::string NormalizeLoggerNameForFile(const std::string &logger_name) {
  std::string base_name = ExtractBasename(logger_name);
  if (base_name.empty()) {
    return "default";
  }

  std::vector<std::string> tokens = Split(base_name, '_');
  if (tokens.empty()) {
    return "default";
  }

  while (tokens.size() > 1 && IsTimestampLike(tokens.back())) {
    tokens.pop_back();
  }

  if (tokens.size() >= 3 && tokens[tokens.size() - 2] == "node" &&
      IsLikelyDynamicToken(tokens.back())) {
    tokens.pop_back();
  }

  if (tokens.size() >= 3 && tokens.back() == "node" &&
      IsLikelyDynamicToken(tokens[tokens.size() - 2])) {
    tokens.erase(tokens.end() - 2);
  }

  return Join(tokens, '_');
}

void InstallProcessWideLogSink() {
  static std::once_flag once;
  std::call_once(once, []() { LogFileManager::Instance().Install(); });
}

}  // namespace common_utils
