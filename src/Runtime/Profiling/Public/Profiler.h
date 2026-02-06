#pragma once

// Tracy Profiler Integration
// Compile with TRACY_ENABLE to activate profiling
// Run the Tracy profiler GUI to capture and visualize traces

#ifdef TRACY_ENABLE
    #include <tracy/Tracy.hpp>
    
    // Frame marker - call once per frame at end of main loop
    #define STRIGID_FRAME_MARK() FrameMark
    
    // Zone profiling - measures time spent in scope
    #define STRIGID_ZONE() ZoneScoped
    #define STRIGID_ZONE_N(name) ZoneScopedN(name)
    
    // Zone with color (for visual grouping in Tracy)
    #define STRIGID_ZONE_C(color) ZoneScopedC(color)
    
    // Zone with dynamic text (e.g., "Processing Entity 42")
    #define STRIGID_ZONE_TEXT(text, size) ZoneText(text, size)
    
    // Memory profiling
    #define STRIGID_ALLOC(ptr, size) TracyAlloc(ptr, size)
    #define STRIGID_FREE(ptr) TracyFree(ptr)
    
    // Plots (for custom metrics like FPS, entity count, etc.)
    #define STRIGID_PLOT(name, value) TracyPlot(name, value)
    
#else
    // No-op macros when Tracy is disabled
    #define STRIGID_FRAME_MARK()
    #define STRIGID_ZONE()
    #define STRIGID_ZONE_N(name)
    #define STRIGID_ZONE_C(color)
    #define STRIGID_ZONE_TEXT(text, size)
    #define STRIGID_ALLOC(ptr, size)
    #define STRIGID_FREE(ptr)
    #define STRIGID_PLOT(name, value)
#endif

// Tracy color definitions (24-bit RGB)
#define STRIGID_COLOR_MEMORY    0xFF6B6B  // Red
#define STRIGID_COLOR_RENDERING 0x4ECDC4  // Cyan
#define STRIGID_COLOR_PHYSICS   0xFFE66D  // Yellow
#define STRIGID_COLOR_LOGIC     0x95E1D3  // Green
#define STRIGID_COLOR_NETWORK   0xF38181  // Pink
#define STRIGID_COLOR_AUDIO     0xAA96DA  // Purple
