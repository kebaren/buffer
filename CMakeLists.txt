cmake_minimum_required(VERSION 3.10)
project(textbuffer VERSION 1.0.0 LANGUAGES CXX)

# Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set default build type to Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Define the library sources - manually list files to avoid including deleted ones
set(SOURCES
    src/rb_tree_base.cpp
    src/piece_tree_builder.cpp
    src/unicode.cpp
    src/line_starts.cpp
    src/piece_tree_base.cpp
    src/piece_tree_snapshot.cpp
    src/textbuffer.cpp
)

file(GLOB_RECURSE HEADERS "include/*.h")

# Create the library
add_library(textbuffer STATIC ${SOURCES} ${HEADERS})
target_include_directories(textbuffer PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Enable ICU for Unicode support if available
find_package(ICU COMPONENTS uc data)
if(ICU_FOUND)
    target_link_libraries(textbuffer PRIVATE ICU::uc ICU::data)
    target_compile_definitions(textbuffer PRIVATE USE_ICU=1)
else()
    message(STATUS "ICU not found, using built-in Unicode support")
    target_compile_definitions(textbuffer PRIVATE USE_ICU=0)
endif()

# Add examples
add_subdirectory(examples)

# Enable testing
enable_testing()
add_subdirectory(tests)

# Installation
install(TARGETS textbuffer
    EXPORT textbuffer-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)
install(DIRECTORY include/ DESTINATION include)

# Export the targets
install(EXPORT textbuffer-targets
    FILE textbuffer-targets.cmake
    NAMESPACE textbuffer::
    DESTINATION lib/cmake/textbuffer
)

# Export the package configuration
include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/textbuffer-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/textbuffer-config.cmake
    INSTALL_DESTINATION lib/cmake/textbuffer
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/textbuffer-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/textbuffer-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/textbuffer-config-version.cmake
    DESTINATION lib/cmake/textbuffer
) 