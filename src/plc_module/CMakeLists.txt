cmake_minimum_required(VERSION 3.8)
project(plc_module)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

option(ENABLE_RZ_VERSION "Enable clearance_node_exec build" OFF)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Boost REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBMODBUS REQUIRED IMPORTED_TARGET libmodbus)

find_package(common_utils REQUIRED)

# ---------- 公共静态库 ----------
add_library(plc_common
  src/plc_device_base.cc
  src/modbus_rtu.cc
  src/modbus_tcp.cc
  src/modbus_rtu_slave.cc
  src/modbus_tcp_slave.cc
  src/transparent_transport_wrapper.cpp
)

# 让静态库可被其它共享库链接
set_target_properties(plc_common PROPERTIES POSITION_INDEPENDENT_CODE ON)

# 头文件对外可见
target_include_directories(plc_common PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
   ${LIBMODBUS_INCLUDE_DIRS} 
)

# 依赖对外可见（PUBLIC），下游无需重复 find
target_link_libraries(plc_common PUBLIC
  ${LIBMODBUS_LIBRARIES}
  ${Boost_LIBRARIES}
)

ament_target_dependencies(plc_common
  PUBLIC
    rclcpp
    std_msgs
    nlohmann_json
    common_utils
)

add_library(plc_module::plc_common ALIAS plc_common)

# ---------- 安装头文件与库 并导出 ----------
install(DIRECTORY include/
  DESTINATION include
)

install(TARGETS plc_common
  EXPORT export_plc_module
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES DESTINATION include
)

# 导出给下游包
ament_export_targets(export_plc_module HAS_LIBRARY_TARGET)
ament_export_include_directories(include)
ament_export_libraries(plc_common)
ament_export_dependencies(
  rclcpp
  std_msgs
  nlohmann_json
  Boost
  common_utils
)

ament_package()
