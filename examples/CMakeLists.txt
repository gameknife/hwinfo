add_executable(system_info system_infoMain.cpp)
target_link_libraries(system_info PRIVATE lfreist-hwinfo::hwinfo)
install(TARGETS system_info)

add_executable(hardware_summary hardware_summaryMain.cpp)
target_link_libraries(hardware_summary PRIVATE lfreist-hwinfo::hwinfo)
install(TARGETS hardware_summary)
