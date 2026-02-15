#pragma once

#include <cstdint>

// Simple cube mesh for instanced rendering
// 8 vertices, 12 triangles (36 indices)
namespace CubeMesh
{
    // Vertex format: Position (vec3)
    struct Vertex
    {
        float x, y, z;
    };

    // Cube vertices (centered at origin, size 1x1x1)
    constexpr Vertex Vertices[] = {
        // Front face
        {-0.5f, -0.5f, 0.5f}, // 0
        {0.5f, -0.5f, 0.5f}, // 1
        {0.5f, 0.5f, 0.5f}, // 2
        {-0.5f, 0.5f, 0.5f}, // 3
        // Back face
        {-0.5f, -0.5f, -0.5f}, // 4
        {0.5f, -0.5f, -0.5f}, // 5
        {0.5f, 0.5f, -0.5f}, // 6
        {-0.5f, 0.5f, -0.5f} // 7
    };

    // Triangle indices (CCW winding)
    constexpr uint16_t Indices[] = {
        // Front
        0, 1, 2, 2, 3, 0,
        // Right
        1, 5, 6, 6, 2, 1,
        // Back
        5, 4, 7, 7, 6, 5,
        // Left
        4, 0, 3, 3, 7, 4,
        // Top
        3, 2, 6, 6, 7, 3,
        // Bottom
        4, 5, 1, 1, 0, 4
    };

    constexpr size_t VertexCount = sizeof(Vertices) / sizeof(Vertex);
    constexpr size_t IndexCount = sizeof(Indices) / sizeof(uint16_t);
}
