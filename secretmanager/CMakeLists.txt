
cmake_minimum_required(VERSION 3.10)

# Define the project name and where to report bugs.
set(PACKAGE_BUGREPORT
    "https://github.com/GoogleCloudPlatform/cpp-samples/issues")
project(cpp-samples-speech-api CXX)

# Configure the Compiler options, we will be using C++11 features.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(google_cloud_cpp_googleapis REQUIRED)
find_package(Threads)

foreach(target secret)
  add_executable("${target}" "${target}.cc")
  target_link_libraries(
    "${target}" PRIVATE google-cloud-cpp::cloud_secretmanager_v1_protos
                        Threads::Threads)
endforeach()
