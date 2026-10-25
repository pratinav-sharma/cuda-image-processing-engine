cmake_minimum_required(VERSION 3.24)
# Use the GPU on the build machine unless the caller explicitly chooses a target.
if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
  set(CMAKE_CUDA_ARCHITECTURES native)
endif()
project(cuda_image_processing_engine VERSION 0.1.0 LANGUAGES CXX CUDA)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

add_library(image_processing src/grayscale.cpp src/grayscale.cu)
target_include_directories(image_processing PUBLIC include)
set_target_properties(image_processing PROPERTIES CUDA_RUNTIME_LIBRARY Static)

add_library(image_io src/image_io.cpp)
target_include_directories(image_io PUBLIC include PRIVATE third_party/stb)

add_executable(image_demo src/main.cpp)
target_link_libraries(image_demo PRIVATE image_processing image_io)

include(CTest)
if(BUILD_TESTING)
  add_executable(image_correctness tests/correctness.cpp)
  target_link_libraries(image_correctness PRIVATE image_processing)
  add_test(NAME grayscale_gpu_matches_cpu COMMAND image_correctness)
  set_tests_properties(grayscale_gpu_matches_cpu PROPERTIES TIMEOUT 60)
endif()
