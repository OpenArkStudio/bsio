cmake_minimum_required(VERSION 3.5)

project (bsio_examples)

include_directories("${PROJECT_SOURCE_DIR}/dep/asio-1-16-1/asio/include")
include_directories("${PROJECT_SOURCE_DIR}/include")

add_subdirectory(examples)