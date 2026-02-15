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
# Usage: cmake -DENABLE_TRACY=OFF -DGENERATE_ASSEMBLY=ON ..
option(ENABLE_TRACY "Enable Tracy profiler" ON)
option(GENERATE_ASSEMBLY "Generate assembly listings (.cod files)" OFF)
option(VECTORIZATION_REPORTS "Enable compiler vectorization reports" OFF)
option(ENABLE_AVX2 "Enable AVX2 instruction set" ON)

# -----------------------------------------------------------------------------
# 3. DEPENDENCIES (Define Paths Early)
# -----------------------------------------------------------------------------
set(SDL3_PATH "${CMAKE_SOURCE_DIR}/libs/SDL3")
set(TRACY_PATH "${CMAKE_SOURCE_DIR}/libs/tracy")

# -----------------------------------------------------------------------------
# 4. TRACY INTEGRATION
# -----------------------------------------------------------------------------
if(ENABLE_TRACY)
    # Tracy profiling level
    # 1 = Coarse (frame/system level, ~1-2% overhead)
    # 2 = Medium (includes per-chunk/subsystem zones, ~5-10% overhead)
    # 3 = Fine (includes per-entity zones, ~50%+ overhead)
    set(TRACY_PROFILE_LEVEL "3" CACHE STRING "Tracy profiling detail level (1=Coarse, 2=Medium, 3=Fine)")
    set_property(CACHE TRACY_PROFILE_LEVEL PROPERTY STRINGS "1" "2" "3")

    # Tracy requires these source files
    set(TRACY_SOURCES
        "${TRACY_PATH}/public/TracyClient.cpp"
    )

    # Add Tracy include directory
    include_directories("${TRACY_PATH}/public")

    # Enable Tracy profiling and disable ROCm support
    add_compile_definitions(TRACY_ENABLE)
    add_compile_definitions(TRACY_PROFILE_LEVEL=${TRACY_PROFILE_LEVEL})
    add_compile_definitions(TRACY_NO_ROCTRACER=1)
    add_compile_definitions(TRACY_NO_ROCPROF=1)

    message(STATUS "Tracy profiler enabled (Level ${TRACY_PROFILE_LEVEL})")
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
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/Rendering/Public"
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
# 8. COMPILER WARNINGS & OPTIMIZATION FLAGS
# -----------------------------------------------------------------------------
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)

    # Assembly listing generation
    if(GENERATE_ASSEMBLY)
        target_compile_options(${PROJECT_NAME} PRIVATE /FAcs)
        message(STATUS "Assembly listings enabled (build/*.cod files)")
    endif()

    # Vectorization reports
    if(VECTORIZATION_REPORTS)
        target_compile_options(${PROJECT_NAME} PRIVATE /Qvec-report:2)
        message(STATUS "Vectorization reports enabled")
    endif()

    # AVX2 instruction set
    if(ENABLE_AVX2)
        target_compile_options(${PROJECT_NAME} PRIVATE /arch:AVX2)
        message(STATUS "AVX2 instructions enabled")
    endif()
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)

    # Assembly listing generation (GCC/Clang)
    if(GENERATE_ASSEMBLY)
        target_compile_options(${PROJECT_NAME} PRIVATE -save-temps=obj -fverbose-asm)
        message(STATUS "Assembly listings enabled (build/*.s files)")
    endif()

    # Vectorization reports (GCC/Clang)
    if(VECTORIZATION_REPORTS)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(${PROJECT_NAME} PRIVATE -Rpass=loop-vectorize -Rpass-missed=loop-vectorize)
        else()
            target_compile_options(${PROJECT_NAME} PRIVATE -fopt-info-vec-optimized -fopt-info-vec-missed)
        endif()
        message(STATUS "Vectorization reports enabled")
    endif()

    # AVX2 instruction set
    if(ENABLE_AVX2)
        target_compile_options(${PROJECT_NAME} PRIVATE -march=native)
        message(STATUS "Native architecture instructions enabled")
    endif()
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
