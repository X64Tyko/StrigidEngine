#pragma once
#include <string>
#include <vector>

#include "MeshAsset.h"
#include "SkinWeightsAsset.h"
#include "SkeletonAsset.h"
#include "AnimationAsset.h"
#include "TnxName.h"

// -----------------------------------------------------------------------
// SkeletonImporter — glTF skeletal mesh import
//
// Imports all skeletal data from a single .gltf or .glb file in one pass:
//   - Mesh geometry   → MeshAsset   (same format as MeshImporter output)
//   - Skin weights    → SkinWeightsAsset  (.tnxskin)
//   - Skeleton        → SkeletonAsset     (.tnxskel)
//   - Animations      → AnimationAsset[]  (.tnxanim per clip)
//
// Call ImportSkeletalGLTF to fill the result, then write each asset to
// disk with its Save function and load into the respective manager.
// -----------------------------------------------------------------------

struct SkeletalImportResult
{
	MeshAsset        mesh;
	SkinWeightsAsset skin;
	SkeletonAsset    skeleton;

	// One AnimationAsset per animation found in the glTF file.
	// animNames[i] matches animations[i] — use for output file naming.
	std::vector<AnimationAsset> animations;
	std::vector<TnxName>        animNames;

	bool IsValid() const
	{
		return mesh.IsValid() && skin.IsValid() && skeleton.boneCount > 0;
	}
};

// Import a single .gltf or .glb file containing skeletal mesh data.
// Returns true on success.  On failure, outResult is partially filled —
// check individual IsValid() calls for per-subsystem success.
bool ImportSkeletalGLTF(const std::string& srcPath, SkeletalImportResult& outResult);
