set(TARGET lib_ros_node)

find_package(catkin REQUIRED COMPONENTS
  roscpp
  message_generation
  std_msgs
  visualization_msgs
  geometry_msgs
  nav_msgs
  mg_msgs
  sensor_msgs
)

set(SOURCE_FILES
  marker_publisher_node.cc
  quad_state_subscriber_node.cc
  quad_state_publisher_node.cc
  trajectory_visualizer_node.cc
  balloon_status_publisher_node.cc
  balloon_status_subscriber_node.cc
  balloon_position_publisher_node.cc
  balloon_position_subscriber_node.cc
  goal_status_publisher_node.cc
  goal_status_subscriber_node.cc
  trajectory_publisher_node.cc
  trajectory_subscriber_node.cc
  trajectory_client.cc
  trajectory_server.cc
  joy_subscriber_node.cc
)

add_library(${TARGET} STATIC ${SOURCE_FILES})

target_include_directories(${TARGET} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${catkin_INCLUDE_DIRS}
)

target_link_libraries(${TARGET} PUBLIC
  lib_mediation_layer
  lib_joy
  ${catkin_LIBRARIES}
)

add_dependencies(${TARGET} std_msgs_generate_messages_cpp)
add_dependencies(${TARGET} visualization_msgs_generate_messages_cpp)
add_dependencies(${TARGET} geometry_msgs_generate_messages_cpp)
add_dependencies(${TARGET} nav_msgs_generate_messages_cpp)
add_dependencies(${TARGET} mg_msgs_generate_messages_cpp)

set_target_properties(${TARGET} PROPERTIES
  CXX_STANDARD 14
  CXX_STANDARD_REQUIRED YES
  CXX_EXTENSIONS NO
)

target_compile_options(${TARGET} PRIVATE
  -Wfatal-errors
)
