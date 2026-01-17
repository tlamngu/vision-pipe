cmake_minimum_required(VERSION 3.16)
project(VisionPipe VERSION 1.2.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================================
# Build Options
# ============================================================================
option(VISIONPIPE_BUILD_CLI "Build VisionPipe CLI executable" ON)
option(VISIONPIPE_BUILD_TESTS "Build VisionPipe tests" OFF)
option(VISIONPIPE_BUILD_EXAMPLES "Build VisionPipe examples" OFF)
option(VISIONPIPE_BUILD_DOCS "Build VisionPipe documentation" OFF)

# Feature flags
option(VISIONPIPE_WITH_FASTCV "Enable FastCV extensions (requires OpenCV with FastCV)" OFF)
option(VISIONPIPE_WITH_ICEORYX2 "Enable Iceoryx2 IPC for frame publishing" OFF)
option(VISIONPIPE_WITH_CUDA "Enable CUDA acceleration" OFF)
option(VISIONPIPE_WITH_OPENCL "Enable OpenCL acceleration" ON)
option(VISIONPIPE_WITH_DNN "Enable DeepLearning/DNN features (requires OpenCV DNN module)" ON)
option(VISIONPIPE_WITH_ONNXRUNTIME "Enable ONNX Runtime backend for DNN (alternative to OpenCV DNN)" ON)
option(VISIONPIPE_IP_CAM "Enable HTTP-based IP streaming (MJPEG) for remote frame access" OFF)
option(LIBCAMERA_BACKEND "Enable native libcamera backend for Linux camera support" OFF)

# Performance options
option(VISIONPIPE_ENABLE_SIMD "Enable SIMD optimizations" ON)
option(VISIONPIPE_ENABLE_LTO "Enable Link-Time Optimization" ON)

# ============================================================================
# Platform Detection
# ============================================================================
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(VISIONPIPE_PLATFORM "windows")
    add_definitions(-DVISIONPIPE_PLATFORM_WINDOWS)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(VISIONPIPE_PLATFORM "linux")
    add_definitions(-DVISIONPIPE_PLATFORM_LINUX)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(VISIONPIPE_PLATFORM "macos")
    add_definitions(-DVISIONPIPE_PLATFORM_MACOS)
else()
    message(WARNING "Unknown platform: ${CMAKE_SYSTEM_NAME}")
    set(VISIONPIPE_PLATFORM "unknown")
endif()

# ============================================================================
# Compiler Configuration
# ============================================================================
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(VISIONPIPE_ENABLE_SIMD)
        add_compile_options(-march=native)
    endif()
    if(VISIONPIPE_ENABLE_LTO)
        add_compile_options(-flto)
        add_link_options(-flto)
    endif()
elseif(MSVC)
    add_compile_options(/W4 /permissive-)
    if(VISIONPIPE_ENABLE_SIMD)
        add_compile_options(/arch:AVX2)
    endif()
    if(VISIONPIPE_ENABLE_LTO)
        add_compile_options(/GL)
        add_link_options(/LTCG)
    endif()
endif()

# ============================================================================
# Find Dependencies
# ============================================================================

# OpenCV - Required
# You can set OpenCV_DIR manually:
#   cmake .. -DOpenCV_DIR=C:/opencv/build
# Or download OpenCV from: https://opencv.org/releases/
find_package(OpenCV COMPONENTS
    core
    imgproc
    imgcodecs
    videoio
    calib3d
    highgui
    dnn
    photo
    stitching
    # xphoto
    video
)

if(NOT OpenCV_FOUND)
    message(FATAL_ERROR 
        "OpenCV not found!\n"
        "Please install OpenCV and set OpenCV_DIR:\n"
        "  1. Download from https://opencv.org/releases/\n"
        "  2. Extract to C:\\opencv\n"
        "  3. Run: cmake .. -DOpenCV_DIR=C:/opencv/build\n"
        "Or use vcpkg: vcpkg install opencv4:x64-windows\n"
        "  Then: cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake"
    )
endif()

message(STATUS "Found OpenCV: ${OpenCV_VERSION}")
message(STATUS "OpenCV libraries: ${OpenCV_LIBS}")
message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")

# Check for OpenCV with OpenCL
if(VISIONPIPE_WITH_OPENCL)
    if(OpenCV_VERSION VERSION_GREATER_EQUAL "4.0")
        add_definitions(-DVISIONPIPE_OPENCL_ENABLED)
        message(STATUS "OpenCL support enabled")
    else()
        message(WARNING "OpenCL requires OpenCV 4.0+, disabling")
        set(VISIONPIPE_WITH_OPENCL OFF)
    endif()
endif()

# FastCV extension check
if(VISIONPIPE_WITH_FASTCV)
    # Check if OpenCV was built with FastCV
    # FastCV is typically available as a Qualcomm SDK
    find_path(FASTCV_INCLUDE_DIR fastcv.h
        HINTS
        $ENV{FASTCV_SDK}/inc
        /usr/local/include
        /opt/fastcv/include
    )
    find_library(FASTCV_LIBRARY fastcv
        HINTS
        $ENV{FASTCV_SDK}/lib
        /usr/local/lib
        /opt/fastcv/lib
    )
    if(FASTCV_INCLUDE_DIR AND FASTCV_LIBRARY)
        add_definitions(-DVISIONPIPE_FASTCV_ENABLED)
        message(STATUS "FastCV support enabled")
    else()
        message(WARNING "FastCV not found, disabling FastCV extensions")
        set(VISIONPIPE_WITH_FASTCV OFF)
    endif()
endif()

# Iceoryx2 for IPC
if(VISIONPIPE_WITH_ICEORYX2)
    # Try to find iceoryx2 using CMake config or pkg-config
    find_package(iceoryx2 QUIET)
    if(NOT iceoryx2_FOUND)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(ICEORYX2 iceoryx2)
        endif()
    endif()
    
    if(iceoryx2_FOUND OR ICEORYX2_FOUND)
        add_definitions(-DVISIONPIPE_ICEORYX2_ENABLED)
        message(STATUS "Iceoryx2 support enabled")
    else()
        message(WARNING "Iceoryx2 not found, disabling IPC extensions")
        set(VISIONPIPE_WITH_ICEORYX2 OFF)
    endif()
endif()

# CUDA support
if(VISIONPIPE_WITH_CUDA)
    find_package(CUDA)
    if(CUDA_FOUND)
        add_definitions(-DVISIONPIPE_CUDA_ENABLED)
        message(STATUS "CUDA support enabled: ${CUDA_VERSION}")
    else()
        message(WARNING "CUDA not found, disabling CUDA extensions")
        set(VISIONPIPE_WITH_CUDA OFF)
    endif()
endif()

# LibCamera support (Linux only)
if(LIBCAMERA_BACKEND)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(WARNING "libcamera is only supported on Linux, disabling LIBCAMERA_BACKEND")
        set(LIBCAMERA_BACKEND OFF)
    else()
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(LIBCAMERA libcamera)
        if(LIBCAMERA_FOUND)
            add_definitions(-DVISIONPIPE_LIBCAMERA_ENABLED)
            message(STATUS "libcamera support enabled: ${LIBCAMERA_VERSION}")
        else()
            message(WARNING "libcamera not found, disabling LIBCAMERA_BACKEND")
            set(LIBCAMERA_BACKEND OFF)
        endif()
    endif()
endif()

# Threading support
find_package(Threads REQUIRED)

# ============================================================================
# Source Files
# ============================================================================

set(VISIONPIPE_CORE_SOURCES
    src/pipeline/pipeline.cpp
    src/pipeline/pipeline_item.cpp
    src/pipeline/pipeline_threaded_group.cpp
    src/utils/Logger.cpp
)

set(VISIONPIPE_INTERPRETER_SOURCES
    src/interpreter/lexer.cpp
    src/interpreter/parser.cpp
    src/interpreter/ast.cpp
    src/interpreter/interpreter.cpp
    src/interpreter/item_registry.cpp
    src/interpreter/runtime.cpp
    src/interpreter/cache_manager.cpp
)

# OpenCV API item implementations (modular)
set(VISIONPIPE_ITEMS_SOURCES
    src/interpreter/items/all_items.cpp
    src/interpreter/items/video_io_items.cpp
    src/interpreter/items/filter_items.cpp
    src/interpreter/items/color_items.cpp
    src/interpreter/items/transform_items.cpp
    src/interpreter/items/morphology_items.cpp
    src/interpreter/items/edge_items.cpp
    src/interpreter/items/feature_items.cpp
    src/interpreter/items/stereo_items.cpp
    src/interpreter/items/draw_items.cpp
    src/interpreter/items/display_items.cpp
    src/interpreter/items/control_items.cpp
    src/interpreter/items/arithmetic_items.cpp
    src/interpreter/items/advanced_items.cpp
    src/interpreter/items/math_eval_items.cpp
    src/interpreter/items/conditional_items.cpp
    src/interpreter/items/gui_enhanced_items.cpp
    src/interpreter/items/tensor_items.cpp
    src/interpreter/items/ip_stream_items.cpp
)

# IP Stream sources (optional feature)
if(VISIONPIPE_IP_CAM)
    list(APPEND VISIONPIPE_CORE_SOURCES
        src/utils/http_frame_server.cpp
    )
    add_definitions(-DVISIONPIPE_IP_STREAM_ENABLED)
    message(STATUS "IP Stream (HTTP MJPEG streaming) enabled")
endif()

# Camera Device Manager (always included for refactored camera handling)
list(APPEND VISIONPIPE_CORE_SOURCES
    src/utils/camera_device_manager.cpp
)

# LibCamera sources (optional feature)
if(LIBCAMERA_BACKEND)
    list(APPEND VISIONPIPE_ITEMS_SOURCES
        src/interpreter/items/libcamera_items.cpp
    )
    message(STATUS "libcamera items enabled")
endif()

set(VISIONPIPE_CLI_SOURCES
    src/cli/main.cpp
    src/cli/doc_gen.cpp
)

# Optional extension sources
# if(VISIONPIPE_WITH_FASTCV)
#     list(APPEND VISIONPIPE_CORE_SOURCES
#         src/extensions/fastcv/fastcv_items.cpp
#     )
# endif()

# if(VISIONPIPE_WITH_ICEORYX2)
#     list(APPEND VISIONPIPE_CORE_SOURCES
#         src/extensions/iceoryx/iceoryx_publisher.cpp
#         src/extensions/iceoryx/publish_frame_item.cpp
#     )
# endif()

# DeepLearning/DNN sources (optional feature)
if(VISIONPIPE_WITH_DNN)
    set(VISIONPIPE_DNN_SOURCES
        src/interpreter/ml/ml_model.cpp
        src/interpreter/ml/model_registry.cpp
        src/interpreter/ml/opencv_dnn_backend.cpp
        src/interpreter/ml/preprocessing.cpp
        src/interpreter/ml/postprocessing.cpp
        src/interpreter/items/dnn_items.cpp
    )
    list(APPEND VISIONPIPE_ITEMS_SOURCES ${VISIONPIPE_DNN_SOURCES})
    add_definitions(-DVISIONPIPE_WITH_DNN)
    message(STATUS "DeepLearning/DNN features enabled")
endif()

# ============================================================================
# ONNX Runtime Backend (Optional - Alternative to OpenCV DNN)
# ============================================================================
if(VISIONPIPE_WITH_ONNXRUNTIME)
    # Require DNN to be enabled
    if(NOT VISIONPIPE_WITH_DNN)
        message(STATUS "Enabling DNN features (required for ONNX Runtime)")
        set(VISIONPIPE_WITH_DNN ON)
        set(VISIONPIPE_DNN_SOURCES
            src/interpreter/ml/ml_model.cpp
            src/interpreter/ml/model_registry.cpp
            src/interpreter/ml/opencv_dnn_backend.cpp
            src/interpreter/ml/preprocessing.cpp
            src/interpreter/ml/postprocessing.cpp
            src/interpreter/items/dnn_items.cpp
        )
        list(APPEND VISIONPIPE_ITEMS_SOURCES ${VISIONPIPE_DNN_SOURCES})
        add_definitions(-DVISIONPIPE_WITH_DNN)
    endif()
    
    # Find ONNX Runtime
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
    find_package(OnnxRuntime)
    
    if(OnnxRuntime_FOUND)
        add_definitions(-DVISIONPIPE_WITH_ONNXRUNTIME)
        message(STATUS "ONNX Runtime backend enabled (API version ${OnnxRuntime_VERSION})")
        
        # Add ONNX Runtime source file
        list(APPEND VISIONPIPE_ITEMS_SOURCES
            src/interpreter/ml/onnx_runtime_backend.cpp
        )
        
        # Check for execution providers
        if(OnnxRuntime_CUDA_FOUND)
            add_definitions(-DVISIONPIPE_ONNX_CUDA)
            message(STATUS "  - CUDA Execution Provider: Available")
        endif()
        
        if(OnnxRuntime_TensorRT_FOUND)
            add_definitions(-DVISIONPIPE_ONNX_TENSORRT)
            message(STATUS "  - TensorRT Execution Provider: Available")
        endif()
        
        if(OnnxRuntime_DML_FOUND)
            add_definitions(-DVISIONPIPE_ONNX_DML)
            message(STATUS "  - DirectML Execution Provider: Available")
        endif()
        
        if(OnnxRuntime_OpenVINO_FOUND)
            add_definitions(-DVISIONPIPE_ONNX_OPENVINO)
            message(STATUS "  - OpenVINO Execution Provider: Available")
        endif()
    else()
        message(WARNING "ONNX Runtime not found, disabling ONNX Runtime backend")
        message(STATUS "  To enable, set ONNXRUNTIME_ROOT to your ONNX Runtime installation")
        message(STATUS "  Example: cmake -DONNXRUNTIME_ROOT=C:/onnxruntime ...")
        set(VISIONPIPE_WITH_ONNXRUNTIME OFF)
    endif()
endif()

# ============================================================================
# Library Target - Core Pipeline
# ============================================================================
add_library(visionpipe_core STATIC ${VISIONPIPE_CORE_SOURCES})
target_include_directories(visionpipe_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(visionpipe_core PUBLIC
    ${OpenCV_LIBS}
    Threads::Threads
)

# Link Winsock for IP Stream on Windows
if(VISIONPIPE_IP_CAM AND WIN32)
    target_link_libraries(visionpipe_core PRIVATE ws2_32 iphlpapi)
endif()

if(VISIONPIPE_WITH_FASTCV)
    target_include_directories(visionpipe_core PRIVATE ${FASTCV_INCLUDE_DIR})
    target_link_libraries(visionpipe_core PRIVATE ${FASTCV_LIBRARY})
endif()

if(VISIONPIPE_WITH_ICEORYX2)
    if(iceoryx2_FOUND)
        target_link_libraries(visionpipe_core PRIVATE iceoryx2::iceoryx2)
    elseif(ICEORYX2_FOUND)
        target_include_directories(visionpipe_core PRIVATE ${ICEORYX2_INCLUDE_DIRS})
        target_link_libraries(visionpipe_core PRIVATE ${ICEORYX2_LIBRARIES})
    endif()
endif()

# Link libcamera
if(LIBCAMERA_BACKEND)
    target_include_directories(visionpipe_core PRIVATE ${LIBCAMERA_INCLUDE_DIRS})
    target_link_libraries(visionpipe_core PRIVATE ${LIBCAMERA_LIBRARIES})
endif()

# ============================================================================
# Library Target - Interpreter
# ============================================================================
add_library(visionpipe_interpreter STATIC 
    ${VISIONPIPE_INTERPRETER_SOURCES}
    ${VISIONPIPE_ITEMS_SOURCES}
)
target_include_directories(visionpipe_interpreter PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(visionpipe_interpreter PUBLIC
    visionpipe_core
)

# Link ONNX Runtime if available
if(VISIONPIPE_WITH_ONNXRUNTIME AND OnnxRuntime_FOUND)
    target_include_directories(visionpipe_interpreter PRIVATE ${OnnxRuntime_INCLUDE_DIRS})
    target_link_libraries(visionpipe_interpreter PRIVATE ${OnnxRuntime_LIBRARIES})
    message(STATUS "Linking ONNX Runtime libraries: ${OnnxRuntime_LIBRARIES}")
endif()

# ============================================================================
# CLI Executable
# ============================================================================
if(VISIONPIPE_BUILD_CLI)
    add_executable(visionpipe ${VISIONPIPE_CLI_SOURCES})
    target_include_directories(visionpipe PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(visionpipe PRIVATE
        visionpipe_interpreter
        visionpipe_core
    )
    
    # Copy OpenCV DLLs to output directory on Windows
    if(WIN32 AND OpenCV_FOUND)
        # Get OpenCV library path and derive bin directory
        list(GET OpenCV_LIBS 0 FIRST_OPENCV_LIB)
        get_target_property(OpenCV_LIB_LOCATION ${FIRST_OPENCV_LIB} LOCATION)
        get_filename_component(OpenCV_LIB_DIR "${OpenCV_LIB_LOCATION}" DIRECTORY)
        get_filename_component(OpenCV_BIN_DIR "${OpenCV_LIB_DIR}/../bin" ABSOLUTE)
        
        # Find OpenCV world DLL
        file(GLOB OpenCV_WORLD_DLLS "${OpenCV_BIN_DIR}/opencv_world*.dll")
        
        if(OpenCV_WORLD_DLLS)
            foreach(dll ${OpenCV_WORLD_DLLS})
                get_filename_component(dll_name "${dll}" NAME)
                # Only copy release DLL (without 'd' suffix) for Release builds
                if(NOT dll_name MATCHES "d\\.dll$")
                    add_custom_command(TARGET visionpipe POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${dll}"
                            $<TARGET_FILE_DIR:visionpipe>
                        COMMENT "Copying ${dll_name} to output directory"
                    )
                endif()
            endforeach()
        else()
            message(WARNING "OpenCV DLLs not found in ${OpenCV_BIN_DIR}")
        endif()
    endif()

    # Copy ONNX Runtime DLLs to output directory on Windows
    if(WIN32 AND VISIONPIPE_WITH_ONNXRUNTIME AND OnnxRuntime_FOUND)
        # Get the ONNX Runtime bin directory from the library path
        get_filename_component(OnnxRuntime_LIB_DIR "${OnnxRuntime_LIBRARY}" DIRECTORY)
        get_filename_component(OnnxRuntime_ROOT_DIR "${OnnxRuntime_LIB_DIR}/.." ABSOLUTE)
        set(OnnxRuntime_BIN_DIR "${OnnxRuntime_ROOT_DIR}/bin")
        
        # Also check if DLLs are in lib directory (some packages structure)
        if(NOT EXISTS "${OnnxRuntime_BIN_DIR}")
            set(OnnxRuntime_BIN_DIR "${OnnxRuntime_LIB_DIR}")
        endif()
        
        # Find all ONNX Runtime DLLs
        file(GLOB OnnxRuntime_DLLS 
            "${OnnxRuntime_BIN_DIR}/onnxruntime*.dll"
            "${OnnxRuntime_LIB_DIR}/onnxruntime*.dll"
        )
        
        # Also look for provider DLLs
        file(GLOB OnnxRuntime_PROVIDER_DLLS 
            "${OnnxRuntime_BIN_DIR}/onnxruntime_providers_*.dll"
            "${OnnxRuntime_LIB_DIR}/onnxruntime_providers_*.dll"
        )
        list(APPEND OnnxRuntime_DLLS ${OnnxRuntime_PROVIDER_DLLS})
        
        # Remove duplicates
        list(REMOVE_DUPLICATES OnnxRuntime_DLLS)
        
        if(OnnxRuntime_DLLS)
            foreach(dll ${OnnxRuntime_DLLS})
                get_filename_component(dll_name "${dll}" NAME)
                add_custom_command(TARGET visionpipe POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${dll}"
                        $<TARGET_FILE_DIR:visionpipe>
                    COMMENT "Copying ${dll_name} to output directory"
                )
            endforeach()
            message(STATUS "ONNX Runtime DLLs will be copied from: ${OnnxRuntime_BIN_DIR}")
        else()
            message(WARNING "ONNX Runtime DLLs not found. Looking in: ${OnnxRuntime_BIN_DIR}")
            # Try to use the explicitly found DLL
            if(OnnxRuntime_DLL)
                get_filename_component(dll_name "${OnnxRuntime_DLL}" NAME)
                add_custom_command(TARGET visionpipe POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${OnnxRuntime_DLL}"
                        $<TARGET_FILE_DIR:visionpipe>
                    COMMENT "Copying ${dll_name} to output directory"
                )
                message(STATUS "Will copy ONNX Runtime DLL: ${OnnxRuntime_DLL}")
            endif()
        endif()
    endif()

    # Add executable output directory to PATH automatically
    # For multi-config generators (VS, Xcode), output goes to ${CMAKE_BINARY_DIR}/<CONFIG>
    # For single-config generators, output goes to ${CMAKE_BINARY_DIR}
    if(WIN32)
        message("Auto path update was deprecated.")
    else()
        # Linux/macOS: Generate script at build time to get correct output path
        message("Auto path update was deprecated.")        
    endif()

    # Install CLI to appropriate system location
    if(WIN32)
        # Windows: install to Program Files
        install(TARGETS visionpipe
            RUNTIME DESTINATION "bin"
            COMPONENT visionpipe_cli
        )
    else()
        # Linux/macOS: install to /usr/local/bin
        install(TARGETS visionpipe
            RUNTIME DESTINATION "/usr/local/bin"
            COMPONENT visionpipe_cli
        )
    endif()
endif()

# ============================================================================
# Install Configuration
# ============================================================================
install(TARGETS visionpipe_core visionpipe_interpreter
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include/visionpipe
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# ============================================================================
# Package Configuration
# ============================================================================
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/VisionPipeConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/VisionPipeConfig.cmake
    INSTALL_DESTINATION lib/cmake/VisionPipe
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/VisionPipeConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/VisionPipeConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/VisionPipeConfigVersion.cmake
    DESTINATION lib/cmake/VisionPipe
)

# ============================================================================
# Summary
# ============================================================================
message(STATUS "")
message(STATUS "=== VisionPipe Build Configuration ===")
message(STATUS "Version:          ${PROJECT_VERSION}")
message(STATUS "Platform:         ${VISIONPIPE_PLATFORM}")
message(STATUS "Build type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "")
message(STATUS "Features:")
message(STATUS "  CLI:            ${VISIONPIPE_BUILD_CLI}")
message(STATUS "  Tests:          ${VISIONPIPE_BUILD_TESTS}")
message(STATUS "  FastCV:         ${VISIONPIPE_WITH_FASTCV}")
message(STATUS "  Iceoryx2:       ${VISIONPIPE_WITH_ICEORYX2}")
message(STATUS "  CUDA:           ${VISIONPIPE_WITH_CUDA}")
message(STATUS "  OpenCL:         ${VISIONPIPE_WITH_OPENCL}")
message(STATUS "  DeepLearning:   ${VISIONPIPE_WITH_DNN}")
message(STATUS "  ONNX Runtime:   ${VISIONPIPE_WITH_ONNXRUNTIME}")
message(STATUS "  IP Stream:      ${VISIONPIPE_IP_CAM}")
message(STATUS "  SIMD:           ${VISIONPIPE_ENABLE_SIMD}")
message(STATUS "  LTO:            ${VISIONPIPE_ENABLE_LTO}")
message(STATUS "========================================")
message(STATUS "")
