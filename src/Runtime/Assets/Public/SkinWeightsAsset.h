#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "VertexFormat.h"

// -----------------------------------------------------------------------
// .tnxskin binary format
//
// Parallel to .tnxmesh — one SkinWeights entry per vertex, same order.
//
// 64-byte header (one cache line):
//   Offset  Size  Field
//   0       4     Magic: "SKIN" (0x4E494B53)
//   4       4     Version: 1
//   8       4     VertexCount — must match associated .tnxmesh VertexCount
//   12      52    Reserved (pad to 64)
//   64      V     SkinWeights[VertexCount] (8 bytes each)
// -----------------------------------------------------------------------

constexpr uint32_t TnxSkinMagic   = 0x4E494B53; // "SKIN" little-endian
constexpr uint32_t TnxSkinVersion = 1;

struct TnxSkinHeader
{
	uint32_t Magic       = TnxSkinMagic;
	uint32_t Version     = TnxSkinVersion;
	uint32_t VertexCount = 0;
	uint8_t  Reserved[52]{};
};

static_assert(sizeof(TnxSkinHeader) == 64, "TnxSkinHeader must be exactly 64 bytes");

struct SkinWeightsAsset
{
	std::vector<SkinWeights> Weights;
	bool IsValid() const { return !Weights.empty(); }
};

bool SaveSkinWeightsAsset(const SkinWeightsAsset& asset, const std::string& path);
bool LoadSkinWeightsAsset(SkinWeightsAsset& outAsset, const std::string& path);
