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
# 2. DEPENDENCIES (Define Paths Early)
# -----------------------------------------------------------------------------
set(SDL3_PATH "${CMAKE_SOURCE_DIR}/libs/SDL3")

# -----------------------------------------------------------------------------
# 3. SOURCE MANAGEMENT
# -----------------------------------------------------------------------------
# Finds all files in src/ and include/ automatically.
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h" "include/*.hpp")

# CRITICAL CHECK:
# If your main.cpp is in the ROOT folder, add it here manually.
# If it is inside src/, the GLOB_RECURSE above already caught it.
# list(APPEND SOURCES "main.cpp") 

# -----------------------------------------------------------------------------
# 4. EXECUTABLE DEFINITION (The One and Only)
# -----------------------------------------------------------------------------
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
)

# -----------------------------------------------------------------------------
# 5. INCLUDES & LINKS
# -----------------------------------------------------------------------------
# A. Your Engine Headers (Public)
target_include_directories(${PROJECT_NAME} PUBLIC 
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# B. SDL3 Headers (Private)
target_include_directories(${PROJECT_NAME} PRIVATE 
    "${SDL3_PATH}/include"
)

# C. Link Libraries
target_link_libraries(${PROJECT_NAME} PRIVATE 
    "${SDL3_PATH}/lib/x64/SDL3.lib"
)

# -----------------------------------------------------------------------------
# 6. COMPILER WARNINGS
# -----------------------------------------------------------------------------
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# -----------------------------------------------------------------------------
# 7. POST-BUILD STEPS
# -----------------------------------------------------------------------------
# Copy SDL3.dll to the executable folder automatically
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${SDL3_PATH}/lib/x64/SDL3.dll"
    $<TARGET_FILE_DIR:${PROJECT_NAME}>
)