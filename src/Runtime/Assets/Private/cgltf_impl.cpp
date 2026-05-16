// cgltf single-compilation-unit implementation.
// MeshImporter.cpp and SkeletonImporter.cpp both #include "cgltf.h"
// without defining CGLTF_IMPLEMENTATION — this file provides the one definition.
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
