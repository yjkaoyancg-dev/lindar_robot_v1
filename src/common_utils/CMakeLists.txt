cmake_minimum_required(VERSION 3.8)
project(common_utils)

find_package(ament_cmake REQUIRED)
find_package(ament_cmake_gtest REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rcutils REQUIRED)
find_package(nlohmann_json REQUIRED)


add_library(common_utils STATIC
  src/dummy.cpp
  src/logging_sink.cpp
)

target_include_directories(common_utils PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
ament_target_dependencies(common_utils
  PUBLIC
    rclcpp
    rcutils
    nlohmann_json
)

install(TARGETS common_utils
  EXPORT export_common_utils
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES DESTINATION include
)

ament_export_include_directories(include)
ament_export_targets(export_common_utils HAS_LIBRARY_TARGET)
ament_export_libraries(common_utils)
ament_export_dependencies(rclcpp rcutils nlohmann_json)

# 安装头文件
install(DIRECTORY include/
  DESTINATION include
)

if(BUILD_TESTING)
  ament_add_gtest(common_utils_logging_sink_test
    test/logging_sink_test.cpp
  )
  target_link_libraries(common_utils_logging_sink_test
    common_utils
  )
endif()

ament_package()
