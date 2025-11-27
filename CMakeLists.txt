cmake_minimum_required(VERSION 3.15)
project(GameMemoryReader)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# User-space application
add_executable(GameMemoryReader
    src/main.cpp
    src/MemoryReader.cpp
    src/PatternScanner.cpp
    src/OffsetFinder.cpp
    src/EntityTracker.cpp
    src/VulnerableDriverExploit.cpp
)

target_include_directories(GameMemoryReader PRIVATE
    src
    driver
)

target_link_libraries(GameMemoryReader
    psapi
)

# Driver build (requires Windows Driver Kit)
if(WIN32)
    add_custom_target(DriverBuild
        COMMAND ${CMAKE_COMMAND} -E echo "Building kernel driver requires Windows Driver Kit (WDK)"
        COMMAND ${CMAKE_COMMAND} -E echo "Use: cd driver && build -cZ"
    )
endif()

# Copy driver header for user-space
configure_file(
    ${CMAKE_SOURCE_DIR}/driver/kernel_driver.h
    ${CMAKE_BINARY_DIR}/kernel_driver.h
    COPYONLY
)
