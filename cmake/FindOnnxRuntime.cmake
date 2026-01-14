# FindOnnxRuntime.cmake
# Finds the ONNX Runtime library
#
# This module defines:
#   OnnxRuntime_FOUND        - True if ONNX Runtime was found
#   OnnxRuntime_INCLUDE_DIRS - Include directories for ONNX Runtime
#   OnnxRuntime_LIBRARIES    - Libraries to link against
#   OnnxRuntime_VERSION      - Version of ONNX Runtime found
#   OnnxRuntime::OnnxRuntime - Imported target
#
# Optional components:
#   OnnxRuntime_CUDA_FOUND     - CUDA execution provider available
#   OnnxRuntime_TensorRT_FOUND - TensorRT execution provider available
#   OnnxRuntime_DML_FOUND      - DirectML execution provider available (Windows)
#
# Hints:
#   ONNXRUNTIME_ROOT - Root directory of ONNX Runtime installation
#   ENV{ONNXRUNTIME_ROOT} - Environment variable for root directory

# Search paths
set(_OnnxRuntime_ROOT_HINTS
    ${ONNXRUNTIME_ROOT}
    $ENV{ONNXRUNTIME_ROOT}
    $ENV{ORT_ROOT}
    $ENV{ONNXRUNTIME_DIR}
)

# Platform-specific default paths
if(WIN32)
    list(APPEND _OnnxRuntime_ROOT_HINTS
        "C:/Program Files/onnxruntime"
        "C:/onnxruntime"
        "${CMAKE_SOURCE_DIR}/external/onnxruntime"
        "${CMAKE_SOURCE_DIR}/../onnxruntime"
    )
else()
    list(APPEND _OnnxRuntime_ROOT_HINTS
        "/usr/local"
        "/opt/onnxruntime"
        "${CMAKE_SOURCE_DIR}/external/onnxruntime"
        "$ENV{HOME}/onnxruntime"
    )
endif()

# Find include directory
find_path(OnnxRuntime_INCLUDE_DIR
    NAMES onnxruntime_cxx_api.h
    HINTS ${_OnnxRuntime_ROOT_HINTS}
    PATH_SUFFIXES 
        include 
        include/onnxruntime
        include/onnxruntime/core/session
        onnxruntime/core/session
)

# Find main library
if(WIN32)
    find_library(OnnxRuntime_LIBRARY
        NAMES onnxruntime
        HINTS ${_OnnxRuntime_ROOT_HINTS}
        PATH_SUFFIXES lib lib64 bin
    )
    
    # Also find the DLL for runtime
    find_file(OnnxRuntime_DLL
        NAMES onnxruntime.dll
        HINTS ${_OnnxRuntime_ROOT_HINTS}
        PATH_SUFFIXES bin lib
    )
else()
    find_library(OnnxRuntime_LIBRARY
        NAMES onnxruntime
        HINTS ${_OnnxRuntime_ROOT_HINTS}
        PATH_SUFFIXES lib lib64
    )
endif()

# Check for optional execution provider libraries
# CUDA Provider
find_library(OnnxRuntime_CUDA_LIBRARY
    NAMES onnxruntime_providers_cuda
    HINTS ${_OnnxRuntime_ROOT_HINTS}
    PATH_SUFFIXES lib lib64 bin
)

# TensorRT Provider
find_library(OnnxRuntime_TensorRT_LIBRARY
    NAMES onnxruntime_providers_tensorrt
    HINTS ${_OnnxRuntime_ROOT_HINTS}
    PATH_SUFFIXES lib lib64 bin
)

# DirectML Provider (Windows only)
if(WIN32)
    find_library(OnnxRuntime_DML_LIBRARY
        NAMES onnxruntime_providers_dml
        HINTS ${_OnnxRuntime_ROOT_HINTS}
        PATH_SUFFIXES lib lib64 bin
    )
endif()

# OpenVINO Provider
find_library(OnnxRuntime_OpenVINO_LIBRARY
    NAMES onnxruntime_providers_openvino
    HINTS ${_OnnxRuntime_ROOT_HINTS}
    PATH_SUFFIXES lib lib64 bin
)

# Version detection from header
if(OnnxRuntime_INCLUDE_DIR)
    # Try to find version from onnxruntime_c_api.h
    set(_version_file "${OnnxRuntime_INCLUDE_DIR}/onnxruntime_c_api.h")
    if(NOT EXISTS "${_version_file}")
        # Try alternative location
        find_file(_version_file_found
            NAMES onnxruntime_c_api.h
            HINTS ${OnnxRuntime_INCLUDE_DIR}
            PATH_SUFFIXES .. ../.. onnxruntime/core/session
            NO_DEFAULT_PATH
        )
        if(_version_file_found)
            set(_version_file "${_version_file_found}")
        endif()
    endif()
    
    if(EXISTS "${_version_file}")
        file(READ "${_version_file}" _ort_header_content)
        
        # Look for ORT_API_VERSION
        string(REGEX MATCH "#define[ \t]+ORT_API_VERSION[ \t]+([0-9]+)" _match "${_ort_header_content}")
        if(_match)
            set(OnnxRuntime_VERSION "${CMAKE_MATCH_1}")
        endif()
    endif()
endif()

# Standard find_package handling
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OnnxRuntime
    REQUIRED_VARS 
        OnnxRuntime_LIBRARY 
        OnnxRuntime_INCLUDE_DIR
    VERSION_VAR OnnxRuntime_VERSION
)

if(OnnxRuntime_FOUND)
    set(OnnxRuntime_INCLUDE_DIRS ${OnnxRuntime_INCLUDE_DIR})
    set(OnnxRuntime_LIBRARIES ${OnnxRuntime_LIBRARY})
    
    # Check for optional providers
    if(OnnxRuntime_CUDA_LIBRARY)
        set(OnnxRuntime_CUDA_FOUND TRUE)
        list(APPEND OnnxRuntime_LIBRARIES ${OnnxRuntime_CUDA_LIBRARY})
        message(STATUS "  ONNX Runtime CUDA provider: Found")
    else()
        set(OnnxRuntime_CUDA_FOUND FALSE)
    endif()
    
    if(OnnxRuntime_TensorRT_LIBRARY)
        set(OnnxRuntime_TensorRT_FOUND TRUE)
        list(APPEND OnnxRuntime_LIBRARIES ${OnnxRuntime_TensorRT_LIBRARY})
        message(STATUS "  ONNX Runtime TensorRT provider: Found")
    else()
        set(OnnxRuntime_TensorRT_FOUND FALSE)
    endif()
    
    if(OnnxRuntime_DML_LIBRARY)
        set(OnnxRuntime_DML_FOUND TRUE)
        list(APPEND OnnxRuntime_LIBRARIES ${OnnxRuntime_DML_LIBRARY})
        message(STATUS "  ONNX Runtime DirectML provider: Found")
    else()
        set(OnnxRuntime_DML_FOUND FALSE)
    endif()
    
    if(OnnxRuntime_OpenVINO_LIBRARY)
        set(OnnxRuntime_OpenVINO_FOUND TRUE)
        list(APPEND OnnxRuntime_LIBRARIES ${OnnxRuntime_OpenVINO_LIBRARY})
        message(STATUS "  ONNX Runtime OpenVINO provider: Found")
    else()
        set(OnnxRuntime_OpenVINO_FOUND FALSE)
    endif()
    
    # Create imported target
    if(NOT TARGET OnnxRuntime::OnnxRuntime)
        add_library(OnnxRuntime::OnnxRuntime SHARED IMPORTED)
        
        if(WIN32)
            set_target_properties(OnnxRuntime::OnnxRuntime PROPERTIES
                IMPORTED_IMPLIB "${OnnxRuntime_LIBRARY}"
                IMPORTED_LOCATION "${OnnxRuntime_DLL}"
                INTERFACE_INCLUDE_DIRECTORIES "${OnnxRuntime_INCLUDE_DIRS}"
            )
        else()
            set_target_properties(OnnxRuntime::OnnxRuntime PROPERTIES
                IMPORTED_LOCATION "${OnnxRuntime_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${OnnxRuntime_INCLUDE_DIRS}"
            )
        endif()
    endif()
endif()

mark_as_advanced(
    OnnxRuntime_INCLUDE_DIR 
    OnnxRuntime_LIBRARY
    OnnxRuntime_CUDA_LIBRARY
    OnnxRuntime_TensorRT_LIBRARY
    OnnxRuntime_DML_LIBRARY
    OnnxRuntime_OpenVINO_LIBRARY
    OnnxRuntime_DLL
)
