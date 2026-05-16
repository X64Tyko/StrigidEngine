#pragma once
#include "ComponentView.h"
#include "SchemaReflector.h"
#include "AssetTypes.h"

// CSkeletonRef — skeletal mesh binding for an entity.
// Volatile: set at spawn, rarely changes, no rollback needed.
// Render group: SkeletonID drives GPU skinning; SkinMeshID provides vertex+skin data.
template <FieldWidth WIDTH = FieldWidth::Scalar>
struct CSkeletonRef : ComponentView<CSkeletonRef, WIDTH>
{
	TNX_VOLATILE_FIELDS(CSkeletonRef, Render, SkeletonID, SkinMeshID)

	UIntProxy<WIDTH> SkeletonID{}; // slot in SkeletonManager (0 = none)
	UIntProxy<WIDTH> SkinMeshID{}; // slot in MeshManager that has skin weights (0 = none)

	static constexpr auto FieldRefTypes = std::array{
		AssetType::SkeletalMesh, // SkeletonID
		AssetType::SkeletalMesh, // SkinMeshID
	};
};

TNX_REGISTER_COMPONENT(CSkeletonRef)
