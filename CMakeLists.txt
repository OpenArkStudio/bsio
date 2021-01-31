cmake_minimum_required(VERSION 3.5)

project (bsio_examples)

if(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++latest")
elseif(UNIX)
    if (CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.0)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
    endif()
endif()

SET(INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/include" CACHE PATH "The directory the headers are installed in")
install(DIRECTORY include/bsio
        DESTINATION "${INCLUDE_INSTALL_DIR}")

include_directories("${PROJECT_SOURCE_DIR}/dep/asio-1-16-1/asio/include")
include_directories("${PROJECT_SOURCE_DIR}/include")
option(bsio_BUILD_EXAMPLES "Build examples" ON)
if (bsio_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif(bsio_BUILD_EXAMPLES)