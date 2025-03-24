zephyr_library_named(tass)
zephyr_library_sources(Temp_Acc_Sense_Service.c)
target_include_directories(tass PUBLIC ${CMAKE_CURRENT_LIST_DIR})