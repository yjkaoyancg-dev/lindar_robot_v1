#include <cstdlib>
#include <string>

#include <gtest/gtest.h>

#include "common_utils/logging_sink.hpp"

namespace {

class ScopedEnvVar {
 public:
  explicit ScopedEnvVar(const char *name) : name_(name) {
    const char *value = std::getenv(name_);
    if (value != nullptr) {
      had_original_ = true;
      original_value_ = value;
    }
  }

  ~ScopedEnvVar() {
    if (had_original_) {
      setenv(name_, original_value_.c_str(), 1);
    } else {
      unsetenv(name_);
    }
  }

 private:
  const char *name_;
  bool had_original_{false};
  std::string original_value_;
};

TEST(LoggingSinkTest, KeepsStaticLoggerNames) {
  EXPECT_EQ(common_utils::NormalizeLoggerNameForFile("collapse_detector"),
            "collapse_detector");
  EXPECT_EQ(common_utils::NormalizeLoggerNameForFile("/yard/realtime_compute_node"),
            "realtime_compute_node");
}

TEST(LoggingSinkTest, StripsKnownDynamicLoggerSuffixes) {
  EXPECT_EQ(common_utils::NormalizeLoggerNameForFile("lidar_node_SN123456"),
            "lidar_node");
  EXPECT_EQ(common_utils::NormalizeLoggerNameForFile(
                "lidar_SNABCDEF_node_20260320112233"),
            "lidar_node");
}

TEST(LoggingSinkTest, UsesEnvDirectoryWhenProvided) {
  ScopedEnvVar photonix_dir("PHOTONIX_LOG_DIR");
  ScopedEnvVar zwkj_dir("ZWKJ_LOG_DIR");

  setenv("PHOTONIX_LOG_DIR", "/tmp/photonix_test_logs", 1);
  unsetenv("ZWKJ_LOG_DIR");

  EXPECT_EQ(common_utils::ResolveLogBaseDirectory(), "/tmp/photonix_test_logs");
}

}  // namespace
