cmake_minimum_required(VERSION 3.8)
project(astramind_core)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

option(ASTRAMIND_ENABLE_COVERAGE "Instrument astramind_core with gcov for line coverage measurement" OFF)
if(ASTRAMIND_ENABLE_COVERAGE)
  add_compile_options(--coverage -O0)
  add_link_options(--coverage)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(astramind_interfaces REQUIRED)
find_package(ament_index_cpp REQUIRED)

if(NOT DEFINED CMAKE_PREFIX_PATH OR NOT LIBTORCH_ROOT)
  set(LIBTORCH_ROOT "/opt/libtorch" CACHE PATH "Path to the LibTorch distribution")
endif()
list(APPEND CMAKE_PREFIX_PATH "${LIBTORCH_ROOT}")
find_package(Torch REQUIRED)

# Torch installs its runtime libraries outside the default linker search path;
# bake the rpath into every target built here so `ros2 run` works without
# manually exporting LD_LIBRARY_PATH.
set(CMAKE_BUILD_RPATH "${TORCH_INSTALL_PREFIX}/lib")
set(CMAKE_INSTALL_RPATH "${TORCH_INSTALL_PREFIX}/lib")
add_compile_options(${TORCH_CXX_FLAGS})

include_directories(include)

add_library(astramind_core_lib
  src/telem_ingest_node.cpp
  src/astro_guardian_node.cpp
  src/astramind_core_node.cpp
)
target_include_directories(astramind_core_lib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(astramind_core_lib "${TORCH_LIBRARIES}")
ament_target_dependencies(astramind_core_lib rclcpp std_msgs astramind_interfaces ament_index_cpp)

add_executable(telem_ingest_node src/telem_ingest_node_main.cpp)
target_link_libraries(telem_ingest_node astramind_core_lib)
ament_target_dependencies(telem_ingest_node rclcpp astramind_interfaces)

add_executable(astro_guardian_node src/astro_guardian_node_main.cpp)
target_link_libraries(astro_guardian_node astramind_core_lib)
ament_target_dependencies(astro_guardian_node rclcpp astramind_interfaces)

add_executable(astramind_core_node src/astramind_core_node_main.cpp)
target_link_libraries(astramind_core_node astramind_core_lib)
ament_target_dependencies(astramind_core_node rclcpp astramind_interfaces)

install(TARGETS
  telem_ingest_node
  astro_guardian_node
  astramind_core_node
  DESTINATION lib/${PROJECT_NAME}
)

install(TARGETS astramind_core_lib
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(DIRECTORY models/
  DESTINATION share/${PROJECT_NAME}/models
)

install(DIRECTORY launch/
  DESTINATION share/${PROJECT_NAME}/launch
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()

  find_package(ament_cmake_gtest REQUIRED)

  ament_add_gtest(test_telem_ingest_node test/test_telem_ingest_node.cpp)
  target_link_libraries(test_telem_ingest_node astramind_core_lib)
  ament_target_dependencies(test_telem_ingest_node rclcpp astramind_interfaces)

  ament_add_gtest(test_astro_guardian_node test/test_astro_guardian_node.cpp)
  target_link_libraries(test_astro_guardian_node astramind_core_lib)
  ament_target_dependencies(test_astro_guardian_node rclcpp astramind_interfaces)

  ament_add_gtest(test_astramind_core_node test/test_astramind_core_node.cpp)
  target_link_libraries(test_astramind_core_node astramind_core_lib)
  ament_target_dependencies(test_astramind_core_node rclcpp astramind_interfaces)
endif()

ament_package()
