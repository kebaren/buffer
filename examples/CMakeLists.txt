# Add new comprehensive test
add_executable(comprehensive_test comprehensive_test.cpp)
target_link_libraries(comprehensive_test PRIVATE textbuffer)

# Add large file test
add_executable(large_file_test large_file_test.cpp)
target_link_libraries(large_file_test PRIVATE textbuffer)

# Add textbuffer benchmark test
add_executable(textbuffer_benchmark textbuffer_benchmark.cpp)
target_link_libraries(textbuffer_benchmark PRIVATE textbuffer)

# Add simple performance test
add_executable(simple_performance_test simple_performance_test.cpp)
target_link_libraries(simple_performance_test PRIVATE textbuffer)

# Add robustness test
add_executable(textbuffer_robustness_test textbuffer_robustness_test.cpp)
target_link_libraries(textbuffer_robustness_test PRIVATE textbuffer)

# Add insert/delete comprehensive test
add_executable(textbuffer_insert_delete_test textbuffer_insert_delete_test.cpp)
target_link_libraries(textbuffer_insert_delete_test PRIVATE textbuffer)

# Add simple buffer test
add_executable(simple_buffer_test simple_buffer_test.cpp)
target_link_libraries(simple_buffer_test PRIVATE textbuffer)

# Add simplest buffer test
add_executable(simplest_buffer_test simplest_buffer_test.cpp)
target_link_libraries(simplest_buffer_test PRIVATE textbuffer)

# Add custom test for insert and delete functions
add_executable(custom_test custom_test.cpp)
target_link_libraries(custom_test textbuffer)

# Add TextBuffer wrapper test
add_executable(textbuffer_test textbuffer_test.cpp)
target_link_libraries(textbuffer_test textbuffer) 