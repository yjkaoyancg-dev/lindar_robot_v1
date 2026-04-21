cmake_minimum_required(VERSION 3.8)
project(robot_plc_crontorl)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(ament_cmake_gtest REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(std_srvs REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBMODBUS REQUIRED IMPORTED_TARGET libmodbus)

add_library(robot_plc_crontorl_lib
  ../plc_module/src/plc_device_base.cc
  ../plc_module/src/modbus_tcp_slave.cc
  ../plc_module/src/modbus_rtu_slave.cc
  src/robot_pose_payload.cc
  src/robot_plc_controller.cc
  src/robot_plc_node.cc
)

target_include_directories(robot_plc_crontorl_lib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  ${CMAKE_CURRENT_SOURCE_DIR}/../plc_module/include
  ${LIBMODBUS_INCLUDE_DIRS}
)

target_link_libraries(robot_plc_crontorl_lib PUBLIC
  ${LIBMODBUS_LIBRARIES}
)

ament_target_dependencies(robot_plc_crontorl_lib
  PUBLIC
    rclcpp
    std_msgs
    std_srvs
    nlohmann_json
)

add_executable(robot_plc_crontorl_node src/main.cpp)
target_link_libraries(robot_plc_crontorl_node PRIVATE robot_plc_crontorl_lib)

install(TARGETS robot_plc_crontorl_lib robot_plc_crontorl_node
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY include/
  DESTINATION include
)

install(DIRECTORY config/
  DESTINATION share/${PROJECT_NAME}/config
)

install(PROGRAMS
  scripts/test_modpoll.sh
  DESTINATION share/${PROJECT_NAME}/scripts
)

if(BUILD_TESTING)
  ament_add_gtest(robot_plc_state_machine_test
    test/robot_plc_state_machine_test.cc
  )
  target_link_libraries(robot_plc_state_machine_test robot_plc_crontorl_lib)

  ament_add_gtest(robot_pose_json_parser_test
    test/robot_pose_json_parser_test.cc
  )
  target_link_libraries(robot_pose_json_parser_test robot_plc_crontorl_lib)
endif()

ament_export_include_directories(include)
ament_export_libraries(robot_plc_crontorl_lib)
ament_export_dependencies(rclcpp std_msgs std_srvs nlohmann_json)

ament_package()
