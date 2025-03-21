find_package(GTest QUIET)

if(GTest_FOUND)
    message(STATUS "GTest found, but tests are currently disabled")
    # Tests are disabled - uncomment below to enable when test files are available
    # add_executable(textbuffer_tests)
    # target_link_libraries(textbuffer_tests PRIVATE 
    #     textbuffer
    #     GTest::GTest
    #     GTest::Main
    # )
    # add_test(NAME textbuffer_tests COMMAND textbuffer_tests)
else()
    message(STATUS "GTest not found, tests will not be compiled")
endif() 