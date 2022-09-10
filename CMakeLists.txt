cmake_minimum_required(VERSION 3.10)
project(sqs-dispatch)
set (CMAKE_CXX_STANDARD 17)

# Locate the aws sdk for c++ package.
find_package(AWSSDK REQUIRED COMPONENTS sqs)
find_package(Boost REQUIRED COMPONENTS program_options)
find_package(ZLIB REQUIRED)

# add the executable
add_executable(sqs-dispatch main.cpp)

target_link_libraries(sqs-dispatch
  PRIVATE
  ${AWSSDK_LINK_LIBRARIES}
  ${Boost_LIBRARIES}
  ZLIB::ZLIB
)
