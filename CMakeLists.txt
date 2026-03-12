cmake_minimum_required(VERSION 3.16)
project(VisionPipe VERSION 1.4.0 LANGUAGES CXX)

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
# When ON, replaces the POSIX shm IPC backend (shm_write/shm_read/exec_fork arena)
# with Iceoryx2 publish/subscribe.  Requires VISIONPIPE_WITH_ICEORYX2=ON.
option(VISIONPIPE_IPC_USE_ICEORYX2 "Use Iceoryx2 as the default IPC transport for shm_write/shm_read (requires VISIONPIPE_WITH_ICEORYX2)" OFF)
option(VISIONPIPE_WITH_CUDA "Enable CUDA acceleration" OFF)
option(VISIONPIPE_WITH_OPENCL "Enable OpenCL acceleration" ON)
option(VISIONPIPE_WITH_DNN "Enable DeepLearning/DNN features (requires OpenCV DNN module)" ON)
option(VISIONPIPE_WITH_ONNXRUNTIME "Enable ONNX Runtime backend for DNN (alternative to OpenCV DNN)" ON)
option(VISIONPIPE_IP_CAM "Enable HTTP-based IP streaming (MJPEG) for remote frame access" OFF)
option(LIBCAMERA_BACKEND "Enable native libcamera backend for Linux camera support" OFF)
option(V4L2_NATIVE_BACKEND "Enable native V4L2 backend (Linux only)" OFF)

# Iceoryx2 path configuration
set(iceoryx2_PATH "" CACHE PATH "Path to Iceoryx2 installation (e.g. /opt/iceoryx2 or /usr/local). If empty, uses system default search paths.")
if(iceoryx2_PATH)
    list(APPEND CMAKE_PREFIX_PATH "${iceoryx2_PATH}")
    message(STATUS "iceoryx2_PATH set to: ${iceoryx2_PATH}")
endif()

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
    objdetect
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
        $ENV{FASTCV_SDK}/libs
        $ENV{FASTCV_SDK}/lib
        /usr/local/lib
        /usr/local/libs
        /opt/fastcv/lib
        /opt/fastcv/libs
    )

    # message(STATUS "ENV{FASTCV_SDK} = [$ENV{FASTCV_SDK}]")
    # message(STATUS "FASTCV_INCLUDE_DIR = [${FASTCV_INCLUDE_DIR}]")
    # message(STATUS "FASTCV_LIBRARY     = [${FASTCV_LIBRARY}]")

    if(FASTCV_INCLUDE_DIR AND FASTCV_LIBRARY)
        add_definitions(-DVISIONPIPE_FASTCV_ENABLED)
        # Find the OpenCV fastcv contrib module library (cv::fastcv:: symbols)
        find_library(OpenCV_FASTCV_LIB opencv_fastcv
            PATHS
                ${OpenCV_DIR}/lib
                ${OpenCV_DIR}/../lib
                /home/radxa/System/opencv/build/lib
            NO_DEFAULT_PATH
        )
        if(OpenCV_FASTCV_LIB)
            message(STATUS "FastCV support enabled (opencv_fastcv: ${OpenCV_FASTCV_LIB})")
        else()
            message(WARNING "libopencv_fastcv not found — cv::fastcv:: symbols may fail to link")
        endif()
    else()
        message(WARNING "FastCV not found, disabling FastCV extensions")
        set(VISIONPIPE_WITH_FASTCV OFF)
    endif()
endif()

# Iceoryx2 for IPC
if(VISIONPIPE_WITH_ICEORYX2)
    # Primary: CMake config  (iceoryx2-cxx package  — lib/cmake/iceoryx2-cxx/)
    find_package(iceoryx2-cxx QUIET)
    if(NOT iceoryx2-cxx_FOUND)
        # Fallback: pkg-config  (extends PKG_CONFIG_PATH if iceoryx2_PATH set)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            if(iceoryx2_PATH)
                set(ENV{PKG_CONFIG_PATH} "${iceoryx2_PATH}/lib/pkgconfig:${iceoryx2_PATH}/libdata/pkgconfig:$ENV{PKG_CONFIG_PATH}")
            endif()
            pkg_check_modules(ICEORYX2_PKG iceoryx2-cxx)
        endif()
    endif()

    if(iceoryx2-cxx_FOUND OR ICEORYX2_PKG_FOUND)
        add_definitions(-DVISIONPIPE_ICEORYX2_ENABLED)
        if(iceoryx2-cxx_FOUND)
            message(STATUS "Iceoryx2 support enabled (via CMake config: iceoryx2-cxx)")
        else()
            message(STATUS "Iceoryx2 support enabled (via pkg-config)")
        endif()

        # ── Runtime IPC backend selection ───────────────────────────────────
        if(VISIONPIPE_IPC_USE_ICEORYX2)
            add_definitions(-DVISIONPIPE_IPC_USE_ICEORYX2)
            message(STATUS "Iceoryx2 selected as default IPC transport "
                           "(shm_write/shm_read → iceoryx2)")
        endif()
    else()
        message(WARNING "Iceoryx2 not found, disabling Iceoryx2 IPC extensions")
        message(STATUS "  To enable Iceoryx2, specify its path: cmake .. -Diceoryx2_PATH=/path/to/iceoryx2")
        set(VISIONPIPE_WITH_ICEORYX2 OFF)
        if(VISIONPIPE_IPC_USE_ICEORYX2)
            message(WARNING "VISIONPIPE_IPC_USE_ICEORYX2 requires Iceoryx2; "
                            "disabling IPC backend switch")
            set(VISIONPIPE_IPC_USE_ICEORYX2 OFF)
        endif()
    endif()
endif()

# Enforce dependency: IPC backend switch needs the Iceoryx2 library
if(VISIONPIPE_IPC_USE_ICEORYX2 AND NOT VISIONPIPE_WITH_ICEORYX2)
    message(FATAL_ERROR
        "VISIONPIPE_IPC_USE_ICEORYX2=ON requires VISIONPIPE_WITH_ICEORYX2=ON")
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
            message(STATUS "  Includes: ${LIBCAMERA_INCLUDE_DIRS}")
            message(STATUS "  Libraries: ${LIBCAMERA_LIBRARIES}")
        else()
            message(WARNING "libcamera not found, disabling LIBCAMERA_BACKEND")
            set(LIBCAMERA_BACKEND OFF)
        endif()
    endif()
endif()

# V4L2 Native Backend (Linux only)
if(V4L2_NATIVE_BACKEND)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(WARNING "V4L2 native backend is only supported on Linux, disabling V4L2_NATIVE_BACKEND")
        set(V4L2_NATIVE_BACKEND OFF)
    else()
        # Simple file-exists check instead of try-compile (known issue)
        # TODO: Implement proper checks.
        if(EXISTS "/usr/include/linux/videodev2.h")
            add_definitions(-DVISIONPIPE_V4L2_NATIVE_ENABLED)
            message(STATUS "V4L2 native backend enabled (found /usr/include/linux/videodev2.h)")
        else()
            message(WARNING "/usr/include/linux/videodev2.h not found, disabling V4L2_NATIVE_BACKEND")
            set(V4L2_NATIVE_BACKEND OFF)
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
    src/utils/shm_frame_transport.cpp
    src/utils/shm_zero_copy.cpp
)

set(VISIONPIPE_INTERPRETER_SOURCES
    src/interpreter/lexer.cpp
    src/interpreter/parser.cpp
    src/interpreter/ast.cpp
    src/interpreter/interpreter.cpp
    src/interpreter/item_registry.cpp
    src/interpreter/runtime.cpp
    src/interpreter/cache_manager.cpp
    src/interpreter/param_store.cpp
    src/interpreter/param_server.cpp
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
    src/interpreter/items/auto_exposure_items.cpp
    src/interpreter/items/adaptive_brightness_items.cpp
    src/interpreter/items/gpu_debayer_items.cpp
    src/interpreter/items/gpu_transform_items.cpp
    src/interpreter/items/frame_sink_items.cpp
    src/interpreter/items/discovery_items.cpp
    src/interpreter/items/stabilization_items.cpp
    src/interpreter/items/gpu_stabilization_items.cpp
    src/interpreter/items/shm_items.cpp
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

# V4L2 Native sources (optional feature)
if(V4L2_NATIVE_BACKEND)
    list(APPEND VISIONPIPE_CORE_SOURCES
        src/utils/v4l2_device_manager.cpp
    )
    list(APPEND VISIONPIPE_ITEMS_SOURCES
        src/interpreter/items/v4l2_items.cpp
    )
    message(STATUS "V4L2 native items enabled")
endif()

set(VISIONPIPE_CLI_SOURCES
    src/cli/main.cpp
    src/cli/doc_gen.cpp
)
# FastCV accelerated items (optional feature)
if(VISIONPIPE_WITH_FASTCV AND FASTCV_INCLUDE_DIR AND FASTCV_LIBRARY)
    list(APPEND VISIONPIPE_ITEMS_SOURCES
        src/interpreter/items/fastcv_items.cpp
    )
    message(STATUS "FastCV items registered")
endif()

if(VISIONPIPE_WITH_ICEORYX2)
    list(APPEND VISIONPIPE_CORE_SOURCES
        src/extensions/iceoryx/iceoryx_publisher.cpp
        src/extensions/iceoryx/iceoryx_frame_transport.cpp
    )
    list(APPEND VISIONPIPE_ITEMS_SOURCES
        src/extensions/iceoryx/publish_frame_item.cpp
    )
    message(STATUS "Iceoryx2 extension sources included")
endif()

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
    # Extension headers are under include/ (already on target via PUBLIC include)
    if(iceoryx2-cxx_FOUND)
        # Prefer the static lib to avoid runtime .so dependency on iceoryx2
        target_link_libraries(visionpipe_core PUBLIC iceoryx2-cxx::static-lib-cxx)
    elseif(ICEORYX2_PKG_FOUND)
        target_include_directories(visionpipe_core PUBLIC ${ICEORYX2_PKG_INCLUDE_DIRS})
        target_link_libraries(visionpipe_core PUBLIC ${ICEORYX2_PKG_LIBRARIES})
    endif()
endif()

# Link libcamera
if(LIBCAMERA_BACKEND)
    target_include_directories(visionpipe_core PUBLIC ${LIBCAMERA_INCLUDE_DIRS})
    target_link_libraries(visionpipe_core PUBLIC ${LIBCAMERA_LIBRARIES})
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

# FastCV items compiled in visionpipe_interpreter need direct link to both
# the native FastCV SDK (${FASTCV_LIBRARY}) and the OpenCV fastcv contrib module
if(VISIONPIPE_WITH_FASTCV AND FASTCV_INCLUDE_DIR AND FASTCV_LIBRARY)
    target_include_directories(visionpipe_interpreter PRIVATE ${FASTCV_INCLUDE_DIR})
    target_link_libraries(visionpipe_interpreter PRIVATE ${FASTCV_LIBRARY})
    if(OpenCV_FASTCV_LIB)
        target_link_libraries(visionpipe_interpreter PRIVATE ${OpenCV_FASTCV_LIB})
    endif()
endif()

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

    # Resolve the RPATH cycle: libopencv_fastcv lives in a different directory
    # than the installed OpenCV libs. Pin its directory explicitly in RPATH so
    # CMake doesn't try to merge conflicting search order constraints.
    if(OpenCV_FASTCV_LIB)
        get_filename_component(_fcv_lib_dir "${OpenCV_FASTCV_LIB}" DIRECTORY)
        set_property(TARGET visionpipe APPEND PROPERTY BUILD_RPATH "${_fcv_lib_dir}")
        set_property(TARGET visionpipe APPEND PROPERTY INSTALL_RPATH "${_fcv_lib_dir}")
    endif()

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
# VS Code Extension Generator Tool
# ============================================================================
# Reads the live ItemRegistry (all built-in VSP functions with their metadata)
# and regenerates:
#   - vscode-vsp/syntaxes/vsp.tmLanguage.json
#   - vscode-vsp/snippets/vsp.json
#
# Usage (after build):
#   ./gen_vscode_ext --out-dir <repo-root>/vscode-vsp
#   ./gen_vscode_ext --list        # print all registered function names
#   ./gen_vscode_ext --dry-run     # preview output sizes without writing
# ============================================================================
add_executable(gen_vscode_ext
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cli/gen_vscode_ext.cpp
)
target_include_directories(gen_vscode_ext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(gen_vscode_ext PRIVATE
    visionpipe_interpreter
    visionpipe_core
)
if(OpenCV_FASTCV_LIB)
    get_filename_component(_gvse_fcv_dir "${OpenCV_FASTCV_LIB}" DIRECTORY)
    set_property(TARGET gen_vscode_ext APPEND PROPERTY BUILD_RPATH "${_gvse_fcv_dir}")
endif()

# Convenience: after a successful build, automatically run the generator to
# keep the extension files up-to-date.  Can be disabled by setting
# VISIONPIPE_AUTOGEN_VSCODE_EXT=OFF.
option(VISIONPIPE_AUTOGEN_VSCODE_EXT "Auto-regenerate vscode-vsp extension after build" ON)
if(VISIONPIPE_AUTOGEN_VSCODE_EXT)
    add_custom_command(TARGET gen_vscode_ext POST_BUILD
        COMMAND gen_vscode_ext
                --out-dir "${CMAKE_CURRENT_SOURCE_DIR}/vscode-vsp"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Regenerating VS Code extension grammar and snippets..."
        VERBATIM
    )
endif()

# ============================================================================
# Shared Library Target - libvisionpipe (client library)
# ============================================================================
option(VISIONPIPE_WITH_LIBVISIONPIPE "Build libvisionpipe shared library (client SDK)" ON)
option(LIB_VISIONPIPE_EXAMPLE "Build libvisionpipe example program" OFF)

if(VISIONPIPE_WITH_LIBVISIONPIPE)
    set(LIBVISIONPIPE_SOURCES
        src/libvisionpipe/frame_transport.cpp
        src/libvisionpipe/libvisionpipe.cpp
        src/libvisionpipe/discovery.cpp
    )

    add_library(visionpipe_client SHARED ${LIBVISIONPIPE_SOURCES})
    target_include_directories(visionpipe_client PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${OpenCV_INCLUDE_DIRS}
    )
    target_link_libraries(visionpipe_client PUBLIC
        ${OpenCV_LIBS}
        Threads::Threads
    )
    # POSIX shared memory: -lrt needed on Linux (on macOS it's in libc)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(visionpipe_client PUBLIC rt)
    endif()
    target_compile_definitions(visionpipe_client PRIVATE LIBVISIONPIPE_BUILDING)

    # ── Iceoryx2 transport backend for libvisionpipe ────────────────────────────
    # When VISIONPIPE_WITH_ICEORYX2=ON (and VISIONPIPE_IPC_USE_ICEORYX2=ON),
    # libvisionpipe subscribes to frame_sink channels via Iceoryx2 instead of
    # POSIX shared memory.
    if(VISIONPIPE_WITH_ICEORYX2)
        target_sources(visionpipe_client PRIVATE
            src/extensions/iceoryx/iceoryx_publisher.cpp
            src/extensions/iceoryx/iceoryx_frame_transport.cpp
        )
        if(iceoryx2-cxx_FOUND)
            target_link_libraries(visionpipe_client PUBLIC iceoryx2-cxx::static-lib-cxx)
        elseif(ICEORYX2_PKG_FOUND)
            target_include_directories(visionpipe_client PUBLIC ${ICEORYX2_PKG_INCLUDE_DIRS})
            target_link_libraries(visionpipe_client PUBLIC ${ICEORYX2_PKG_LIBRARIES})
        endif()
        target_compile_definitions(visionpipe_client PRIVATE VISIONPIPE_ICEORYX2_ENABLED)
        if(VISIONPIPE_IPC_USE_ICEORYX2)
            target_compile_definitions(visionpipe_client PRIVATE VISIONPIPE_IPC_USE_ICEORYX2)
            message(STATUS "libvisionpipe: Iceoryx2 subscribe transport ENABLED "
                           "(VISIONPIPE_IPC_USE_ICEORYX2=ON)")
        else()
            message(STATUS "libvisionpipe: Iceoryx2 available but POSIX shm transport active "
                           "(set VISIONPIPE_IPC_USE_ICEORYX2=ON to switch)")
        endif()
    endif()

    set_target_properties(visionpipe_client PROPERTIES
        OUTPUT_NAME "visionpipe"
        VERSION ${PROJECT_VERSION}
        SOVERSION 1
    )

    # Also link frame_transport and discovery into visionpipe_core so frame_sink
    # and discovery_items can use them without linking against visionpipe_client.
    target_sources(visionpipe_core PRIVATE
        src/libvisionpipe/frame_transport.cpp
        src/libvisionpipe/discovery.cpp
    )
    target_include_directories(visionpipe_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(visionpipe_core PUBLIC rt)
    endif()

    install(TARGETS visionpipe_client
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
    )
    install(DIRECTORY include/libvisionpipe/
        DESTINATION include/libvisionpipe
        FILES_MATCHING PATTERN "*.h"
    )

    message(STATUS "libvisionpipe shared library: ENABLED")

    # Optional: build example program
    if(LIB_VISIONPIPE_EXAMPLE)
        add_executable(libvisionpipe_example examples/example_libvisionpipe.cpp)
        target_include_directories(libvisionpipe_example PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${OpenCV_INCLUDE_DIRS}
        )
        target_link_libraries(libvisionpipe_example PRIVATE
            visionpipe_client
            ${OpenCV_LIBS}
            Threads::Threads
        )
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            target_link_libraries(libvisionpipe_example PRIVATE rt)
        endif()
        set_target_properties(libvisionpipe_example PROPERTIES
            OUTPUT_NAME "libvisionpipe_example"
        )
        message(STATUS "libvisionpipe example: ENABLED")
    endif()
endif()

# ============================================================================
# Python Bindings - pyvisionpipe
# ============================================================================
option(VISIONPIPE_WITH_PYTHON "Build Python bindings (pyvisionpipe)" OFF)

if(VISIONPIPE_WITH_PYTHON)
    if(NOT VISIONPIPE_WITH_LIBVISIONPIPE)
        message(FATAL_ERROR "VISIONPIPE_WITH_PYTHON requires VISIONPIPE_WITH_LIBVISIONPIPE=ON")
    endif()

    find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

    # Auto-discover pybind11 cmake dir from the Python interpreter if not set
    if(NOT DEFINED pybind11_DIR OR pybind11_DIR STREQUAL "")
        execute_process(
            COMMAND "${Python3_EXECUTABLE}" -c "import pybind11; print(pybind11.get_cmake_dir())"
            OUTPUT_VARIABLE _pybind11_cmake_dir
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE _pybind11_result
        )
        if(_pybind11_result EQUAL 0 AND _pybind11_cmake_dir)
            set(pybind11_DIR "${_pybind11_cmake_dir}" CACHE PATH "pybind11 cmake directory (auto-detected)")
            message(STATUS "pybind11 cmake dir (auto-detected): ${pybind11_DIR}")
        endif()
    endif()

    find_package(pybind11 REQUIRED)

    pybind11_add_module(pyvisionpipe python/pyvisionpipe.cpp)
    target_link_libraries(pyvisionpipe PRIVATE
        visionpipe_client
        ${OpenCV_LIBS}
    )
    target_include_directories(pyvisionpipe PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${OpenCV_INCLUDE_DIRS}
    )

    # Install C++ extension to Python site-packages
    install(TARGETS pyvisionpipe
        LIBRARY DESTINATION "${Python3_SITEARCH}"
    )

    # Install the `visionpipe` wrapper package so `import visionpipe` works
    install(DIRECTORY python/visionpipe
        DESTINATION "${Python3_SITEARCH}"
    )

    message(STATUS "Python bindings (pyvisionpipe): ENABLED")
    message(STATUS "  Python: ${Python3_EXECUTABLE} (${Python3_VERSION})")
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
message(STATUS "  VSCode gen:     ${VISIONPIPE_AUTOGEN_VSCODE_EXT}")
message(STATUS "  Tests:          ${VISIONPIPE_BUILD_TESTS}")
message(STATUS "  FastCV:         ${VISIONPIPE_WITH_FASTCV}")
message(STATUS "  Iceoryx2:       ${VISIONPIPE_WITH_ICEORYX2}")
if(VISIONPIPE_IPC_USE_ICEORYX2)
    message(STATUS "  IPC backend:    iceoryx2")
else()
    message(STATUS "  IPC backend:    posix-shm (default)")
endif()
message(STATUS "  CUDA:           ${VISIONPIPE_WITH_CUDA}")
message(STATUS "  OpenCL:         ${VISIONPIPE_WITH_OPENCL}")
message(STATUS "  DeepLearning:   ${VISIONPIPE_WITH_DNN}")
message(STATUS "  ONNX Runtime:   ${VISIONPIPE_WITH_ONNXRUNTIME}")
message(STATUS "  IP Stream:      ${VISIONPIPE_IP_CAM}")
message(STATUS "  SIMD:           ${VISIONPIPE_ENABLE_SIMD}")
message(STATUS "  LTO:            ${VISIONPIPE_ENABLE_LTO}")
message(STATUS "  LibCamera:      ${LIBCAMERA_BACKEND}")
message(STATUS "  V4L2 Native:    ${V4L2_NATIVE_BACKEND}")
message(STATUS "  libvisionpipe:  ${VISIONPIPE_WITH_LIBVISIONPIPE}")
message(STATUS "  libvp example: ${LIB_VISIONPIPE_EXAMPLE}")
message(STATUS "  Python bind:   ${VISIONPIPE_WITH_PYTHON}")
message(STATUS "========================================")
message(STATUS "")
