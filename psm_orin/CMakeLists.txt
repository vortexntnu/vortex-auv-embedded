cmake_minimum_required(VERSION 3.8)
project(psm_orin)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(std_msgs REQUIRED)
find_package(vortex_msgs REQUIRED)
find_package(chrono REQUIRED)

set(LIB_NAME "${PROJECT_NAME}_component")

add_library(${LIB_NAME} SHARED
  src/psm_orin_node.cpp
  src/psm_orin_driver.c
  )

ament_target_dependencies(${LIB_NAME} PUBLIC
  rclcpp
  rclcpp_components
  std_msgs
  chrono
  vortex_msgs
)

rclcpp_components_register_node(
  ${LIB_NAME}
  #PLUGIN "ThrusterInterfaceAUVNode"
  EXECUTABLE ${PROJECT_NAME}_node
)

ament_export_targets(export_${LIB_NAME})

install(TARGETS ${LIB_NAME}
  EXPORT export_${LIB_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
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