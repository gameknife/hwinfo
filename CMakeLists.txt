cmake_minimum_required(VERSION 3.22)

project(hwinfo LANGUAGES CXX)

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
