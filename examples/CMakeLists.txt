cmake_minimum_required(VERSION 3.16)
project(libvisionpipe_example CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies
find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui videoio)
find_package(Threads REQUIRED)

# Option 1: Find installed libvisionpipe
find_library(LIBVISIONPIPE_LIB visionpipe HINTS /usr/local/lib ${CMAKE_BINARY_DIR}/..)
find_path(LIBVISIONPIPE_INC libvisionpipe/libvisionpipe.h HINTS /usr/local/include ${CMAKE_SOURCE_DIR}/../include)

if(LIBVISIONPIPE_LIB AND LIBVISIONPIPE_INC)
    add_executable(demo example_libvisionpipe.cpp)
    target_include_directories(demo PRIVATE ${LIBVISIONPIPE_INC} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(demo PRIVATE ${LIBVISIONPIPE_LIB} ${OpenCV_LIBS} Threads::Threads)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(demo PRIVATE rt)
    endif()
    message(STATUS "Found libvisionpipe: ${LIBVISIONPIPE_LIB}")
else()
    message(WARNING "libvisionpipe not found. Build the main project first with -DVISIONPIPE_WITH_LIBVISIONPIPE=ON")
endif()
