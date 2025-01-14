cmake_minimum_required(VERSION 3.5.0)

set(TARGET lib_autonomy_protocol)

find_package (Eigen3 3.3 NO_MODULE)

set(SOURCE_FILES
  aerial_robotics/example_autonomy_protocol.cc
  aerial_robotics/student_autonomy_protocol.cc
  aerial_robotics/a_star3d.cc
  game_snapshot.cc
  student_game_engine_visualizer.cc
  )

add_library(${TARGET} STATIC ${SOURCE_FILES})

target_include_directories(${TARGET} PUBLIC
  ${PROJECT_SOURCE_DIR}/src/P4/src
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${TARGET} PUBLIC 
  lib_mediation_layer
  lib_environment
  lib_util
  lib_p4
  lib_joy
  ${Boost_LIBRARIES}
  Eigen3::Eigen)

set_target_properties(${TARGET} PROPERTIES
  CXX_STANDARD 14
  CXX_STANDARD_REQUIRED YES
  CXX_EXTENSIONS NO
)

target_compile_options(${TARGET} PRIVATE
  -Wfatal-errors
)

if (RESEARCH)
   add_subdirectory(research-autonomy-protocols)
   add_subdirectory(ta-autonomy-protocol)
endif()
