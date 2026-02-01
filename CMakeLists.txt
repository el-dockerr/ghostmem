cmake_minimum_required(VERSION 3.10)
project(GhostMem VERSION 0.8.0 LANGUAGES CXX C)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add source files
set(GHOSTMEM_SOURCES
    src/ghostmem/GhostMemoryManager.cpp
    src/3rdparty/lz4.c
)

set(GHOSTMEM_HEADERS
    src/ghostmem/GhostMemoryManager.h
    src/ghostmem/GhostAllocator.h
    src/ghostmem/Version.h
    src/3rdparty/lz4.h
)

# Create static library
add_library(ghostmem STATIC ${GHOSTMEM_SOURCES} ${GHOSTMEM_HEADERS})
target_include_directories(ghostmem PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(ghostmem PUBLIC 
    GHOSTMEM_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    GHOSTMEM_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    GHOSTMEM_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# Create shared library
add_library(ghostmem_shared SHARED ${GHOSTMEM_SOURCES} ${GHOSTMEM_HEADERS})
target_include_directories(ghostmem_shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(ghostmem_shared PUBLIC 
    GHOSTMEM_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    GHOSTMEM_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    GHOSTMEM_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)
set_target_properties(ghostmem_shared PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)
if(NOT WIN32)
    set_target_properties(ghostmem_shared PROPERTIES OUTPUT_NAME ghostmem)
endif()

# Platform-specific settings
if(WIN32)
    # Windows-specific flags
    target_compile_definitions(ghostmem PRIVATE _WIN32)
    target_compile_definitions(ghostmem_shared PRIVATE _WIN32)
else()
    # Linux-specific flags
    target_compile_options(ghostmem PRIVATE -pthread)
    target_compile_options(ghostmem_shared PRIVATE -pthread)
    target_link_libraries(ghostmem pthread)
    target_link_libraries(ghostmem_shared pthread)
endif()

# Create executable
add_executable(ghostmem_demo src/main.cpp)
target_link_libraries(ghostmem_demo ghostmem)

# Install targets
install(TARGETS ghostmem ghostmem_shared ghostmem_demo
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${GHOSTMEM_HEADERS}
    DESTINATION include/ghostmem
)

# Enable testing
option(BUILD_TESTS "Build test programs" ON)
if(BUILD_TESTS)
    enable_testing()
    
    # Test executable
    add_executable(ghostmem_tests
        tests/test_main.cpp
        tests/test_basic_allocation.cpp
        tests/test_compression.cpp
        tests/test_lru.cpp
        tests/test_allocator.cpp
        tests/test_version.cpp
        tests/test_threading.cpp
        tests/test_disk_backing.cpp
        tests/test_metrics.cpp
    )
    
    target_link_libraries(ghostmem_tests ghostmem)
    target_include_directories(ghostmem_tests PRIVATE tests)
    
    # Platform-specific threading support
    if(NOT WIN32)
        target_link_libraries(ghostmem_tests pthread)
    endif()
    
    # Add test to CTest
    add_test(NAME ghostmem_tests COMMAND ghostmem_tests)
endif()
