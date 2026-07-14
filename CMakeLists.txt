cmake_minimum_required(VERSION 3.22)

project(hwinfo LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config} "${CMAKE_BINARY_DIR}/bin")
endforeach()

add_library(hwinfo INTERFACE)
target_compile_features(hwinfo INTERFACE cxx_std_17)
target_include_directories(hwinfo INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(WIN32)
    target_link_libraries(hwinfo INTERFACE dxgi setupapi wbemuuid)
elseif(APPLE)
    target_link_libraries(hwinfo INTERFACE "-framework IOKit" "-framework CoreFoundation")
endif()

add_executable(system_info examples/system_infoMain.cpp)
target_link_libraries(system_info PRIVATE hwinfo)

add_executable(hardware_summary examples/hardware_summaryMain.cpp)
target_link_libraries(hardware_summary PRIVATE hwinfo)

add_executable(hardware_requirements examples/requirementsMain.cpp)
target_link_libraries(hardware_requirements PRIVATE hwinfo)

option(HWINFO_BUILD_LOCAL_REQUIREMENTS
       "Build a local-only checker with the GPU catalog and requirements embedded in the executable"
       OFF)
set(HWINFO_LOCAL_GPU_CATALOG
    "${CMAKE_CURRENT_SOURCE_DIR}/data/local/passmark_gpu_catalog.csv"
    CACHE FILEPATH "CSV GPU catalog embedded by the local requirements checker")
set(HWINFO_LOCAL_MIN_CPU_CORES "4" CACHE STRING "Embedded minimum physical CPU core count")
set(HWINFO_LOCAL_MIN_GPU_MODEL "GeForce RTX 3060" CACHE STRING "Embedded minimum GPU model")
set(HWINFO_LOCAL_MIN_MEMORY_GIB "16" CACHE STRING "Embedded minimum RAM in GiB")
option(HWINFO_LOCAL_REQUIRE_SSD "Embed a requirement for at least one SSD" ON)

if(HWINFO_BUILD_LOCAL_REQUIREMENTS)
    if(NOT EXISTS "${HWINFO_LOCAL_GPU_CATALOG}")
        message(FATAL_ERROR
            "HWINFO_BUILD_LOCAL_REQUIREMENTS requires a local GPU catalog: ${HWINFO_LOCAL_GPU_CATALOG}")
    endif()
    if(NOT HWINFO_LOCAL_MIN_CPU_CORES MATCHES "^[0-9]+$")
        message(FATAL_ERROR "HWINFO_LOCAL_MIN_CPU_CORES must be a non-negative integer")
    endif()
    if(NOT HWINFO_LOCAL_MIN_MEMORY_GIB MATCHES "^[0-9]+$")
        message(FATAL_ERROR "HWINFO_LOCAL_MIN_MEMORY_GIB must be a non-negative integer")
    endif()

    file(READ "${HWINFO_LOCAL_GPU_CATALOG}" HWINFO_LOCAL_GPU_CATALOG_CONTENT)
    if(HWINFO_LOCAL_GPU_CATALOG_CONTENT MATCHES "\\)HWI_GPU_DATA\"")
        message(FATAL_ERROR "The GPU catalog contains the reserved raw-string delimiter HWI_GPU_DATA")
    endif()
    if(HWINFO_LOCAL_MIN_GPU_MODEL MATCHES "\\)HWI_GPU_REQ\"")
        message(FATAL_ERROR "The GPU requirement contains the reserved raw-string delimiter HWI_GPU_REQ")
    endif()

    if(HWINFO_LOCAL_REQUIRE_SSD)
        set(HWINFO_LOCAL_REQUIRE_SSD_LITERAL "true")
    else()
        set(HWINFO_LOCAL_REQUIRE_SSD_LITERAL "false")
    endif()

    set(HWINFO_LOCAL_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
    set(HWINFO_LOCAL_GENERATED_HEADER "${HWINFO_LOCAL_GENERATED_DIR}/hwinfo_local_requirements.h")
    file(MAKE_DIRECTORY "${HWINFO_LOCAL_GENERATED_DIR}")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/hwinfo_local_requirements.h.in"
        "${HWINFO_LOCAL_GENERATED_HEADER}"
        @ONLY)

    add_executable(hardware_requirements_embedded examples/embeddedRequirementsMain.cpp)
    target_link_libraries(hardware_requirements_embedded PRIVATE hwinfo)
    target_include_directories(hardware_requirements_embedded PRIVATE "${HWINFO_LOCAL_GENERATED_DIR}")
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(hwinfo_requirements_test tests/requirements_test.cpp)
    target_link_libraries(hwinfo_requirements_test PRIVATE hwinfo)
    add_test(NAME hwinfo_requirements_test COMMAND hwinfo_requirements_test)
endif()
