project(TrinyxParser)

add_library(TrinyxParser SHARED
    Private/TrinyxParser.cpp
)

target_include_directories(TrinyxParser PUBLIC Public)

target_compile_definitions(TrinyxParser PRIVATE TRINYX_PARSER_EXPORTS)

set_target_properties(TrinyxParser PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
)
