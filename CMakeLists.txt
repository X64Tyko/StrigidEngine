cmake_minimum_required(VERSION 3.20)

# Project Name and Language
project(StrigidEngine VERSION 0.1 LANGUAGES CXX)

# -----------------------------------------------------------------------------
# 1. STANDARDS & COMPILER SETTINGS
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -----------------------------------------------------------------------------
# 2. BUILD OPTIONS
# -----------------------------------------------------------------------------
option(ENABLE_TRACY "Enable Tracy profiler" ON)

# -----------------------------------------------------------------------------
# 3. DEPENDENCIES (Define Paths Early)
# -----------------------------------------------------------------------------
set(SDL3_PATH "${CMAKE_SOURCE_DIR}/libs/SDL3")
set(TRACY_PATH "${CMAKE_SOURCE_DIR}/libs/tracy")

# -----------------------------------------------------------------------------
# 4. TRACY INTEGRATION
# -----------------------------------------------------------------------------
if(ENABLE_TRACY)
    # Tracy requires these source files
    set(TRACY_SOURCES
        "${TRACY_PATH}/public/TracyClient.cpp"
    )
    
    # Add Tracy include directory
    include_directories("${TRACY_PATH}/public")
    
    # Enable Tracy profiling
    add_compile_definitions(TRACY_ENABLE)
    
    message(STATUS "Tracy profiler enabled")
else()
    set(TRACY_SOURCES "")
    message(STATUS "Tracy profiler disabled")
endif()

# -----------------------------------------------------------------------------
# 5. SOURCE MANAGEMENT
# -----------------------------------------------------------------------------
# Finds all files in src/ and include/ automatically.
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h" "src/*.h")

# -----------------------------------------------------------------------------
# 6. EXECUTABLE DEFINITION (The One and Only)
# -----------------------------------------------------------------------------
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${TRACY_SOURCES}
)

# -----------------------------------------------------------------------------
# 7. INCLUDES & LINKS
# -----------------------------------------------------------------------------
# A. Your Engine Headers (Public)
target_include_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/Core/Public"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/Memory/Public"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/Profiling/Public"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/Logging/Public"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/Components/Public"
)

# B. SDL3 Headers (Private)
target_include_directories(${PROJECT_NAME} PRIVATE
    "${SDL3_PATH}/include"
)

# C. Link Libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    "${SDL3_PATH}/lib/x64/SDL3.lib"
)

# D. Tracy platform-specific libraries
if(ENABLE_TRACY)
    if(WIN32)
        target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 dbghelp)
    elseif(UNIX)
        target_link_libraries(${PROJECT_NAME} PRIVATE pthread dl)
    endif()
endif()

# -----------------------------------------------------------------------------
# 8. COMPILER WARNINGS
# -----------------------------------------------------------------------------
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# -----------------------------------------------------------------------------
# 9. POST-BUILD STEPS
# -----------------------------------------------------------------------------
# Copy SDL3.dll to the executable folder automatically
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${SDL3_PATH}/lib/x64/SDL3.dll"
    $<TARGET_FILE_DIR:${PROJECT_NAME}>
)
