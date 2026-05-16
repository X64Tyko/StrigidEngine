#include "SkinWeightsAsset.h"
#include "Logger.h"

#include <cstring>
#include <fstream>

bool SaveSkinWeightsAsset(const SkinWeightsAsset& asset, const std::string& path)
{
	if (!asset.IsValid())
	{
		LOG_ENG_ERROR("[SkinWeightsAsset] Cannot save empty asset");
		return false;
	}

	std::ofstream file(path, std::ios::binary);
	if (!file.is_open())
	{
		LOG_ENG_ERROR_F("[SkinWeightsAsset] Failed to open '%s' for writing", path.c_str());
		return false;
	}

	TnxSkinHeader header;
	header.VertexCount = static_cast<uint32_t>(asset.Weights.size());

	file.write(reinterpret_cast<const char*>(&header), sizeof(header));
	file.write(reinterpret_cast<const char*>(asset.Weights.data()),
	           asset.Weights.size() * sizeof(SkinWeights));

	if (!file.good())
	{
		LOG_ENG_ERROR_F("[SkinWeightsAsset] Write error for '%s'", path.c_str());
		return false;
	}

	LOG_ENG_INFO_F("[SkinWeightsAsset] Saved '%s' (%u vertices)", path.c_str(), header.VertexCount);
	return true;
}

bool LoadSkinWeightsAsset(SkinWeightsAsset& outAsset, const std::string& path)
{
	std::ifstream file(path, std::ios::binary);
	if (!file.is_open())
	{
		LOG_ENG_ERROR_F("[SkinWeightsAsset] Failed to open '%s' for reading", path.c_str());
		return false;
	}

	TnxSkinHeader header;
	file.read(reinterpret_cast<char*>(&header), sizeof(header));

	if (header.Magic != TnxSkinMagic)
	{
		LOG_ENG_ERROR_F("[SkinWeightsAsset] Invalid magic in '%s'", path.c_str());
		return false;
	}
	if (header.Version != TnxSkinVersion)
	{
		LOG_ENG_ERROR_F("[SkinWeightsAsset] Unsupported version %u in '%s'", header.Version, path.c_str());
		return false;
	}
	if (header.VertexCount == 0)
	{
		LOG_ENG_ERROR_F("[SkinWeightsAsset] Empty asset in '%s'", path.c_str());
		return false;
	}

	outAsset.Weights.resize(header.VertexCount);
	file.read(reinterpret_cast<char*>(outAsset.Weights.data()),
	          header.VertexCount * sizeof(SkinWeights));

	if (!file.good())
	{
		LOG_ENG_ERROR_F("[SkinWeightsAsset] Read error for '%s'", path.c_str());
		outAsset = {};
		return false;
	}

	LOG_ENG_INFO_F("[SkinWeightsAsset] Loaded '%s' (%u vertices)", path.c_str(), header.VertexCount);
	return true;
}
