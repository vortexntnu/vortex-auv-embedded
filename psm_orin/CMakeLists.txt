cmake_minimum_required(VERSION 3.8)
project(psm_orin)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)

include_directories(include)

add_executable(psm_orin_node
  src/psm_orin_driver.c
  src/psm_orin_ros.cpp
)

ament_target_dependencies(psm_orin_node
  rclcpp
  std_msgs
  spdlog
  fmt
)


target_link_libraries(psm_orin_node fmt::fmt)

install(TARGETS
psm_orin_node
DESTINATION lib/${PROJECT_NAME}
)



install(
  DIRECTORY include/
  DESTINATION include
)

install(DIRECTORY
  launch
  config
  DESTINATION share/${PROJECT_NAME}/
)

ament_package()
