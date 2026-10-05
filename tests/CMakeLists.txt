# Host-only tests
add_executable(test_font test_font.c ../src/font5x7.c)
target_include_directories(test_font PRIVATE ../src)
target_link_libraries(test_font PRIVATE SDL2::SDL2)
set_target_properties(test_font PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
