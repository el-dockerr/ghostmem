cmake_minimum_required(VERSION 3.10)
project(GhostMem VERSION 1.0 LANGUAGES CXX C)

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
    src/3rdparty/lz4.h
)

# Create static library
add_library(ghostmem STATIC ${GHOSTMEM_SOURCES} ${GHOSTMEM_HEADERS})
target_include_directories(ghostmem PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Create shared library
add_library(ghostmem_shared SHARED ${GHOSTMEM_SOURCES} ${GHOSTMEM_HEADERS})
target_include_directories(ghostmem_shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
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

# Enable testing if requested
option(BUILD_TESTS "Build test programs" OFF)
if(BUILD_TESTS)
    enable_testing()
    # Add test subdirectory when available
endif()
