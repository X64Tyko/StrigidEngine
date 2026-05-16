#include "SkinningPass.h"
#include "Logger.h"

#include <cstdio>
#include <cstring>
#include <vector>

static std::vector<uint32_t> ReadSPIRV_Skin(const char* path)
{
	FILE* f = fopen(path, "rb");
	if (!f) return {};
	fseek(f, 0, SEEK_END);
	const long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (size <= 0 || (size % 4) != 0) { fclose(f); return {}; }
	std::vector<uint32_t> code(static_cast<size_t>(size) / 4);
	fread(code.data(), 1, static_cast<size_t>(size), f);
	fclose(f);
	return code;
}

bool SkinningPass::Initialize(VkDevice device, VkPipelineLayout pipelineLayout,
                               VulkanMemory* vkMem, uint32_t maxEntities)
{
	AnimBlendBuffer = vkMem->AllocateBuffer(
		maxEntities * sizeof(GpuAnimEntry),
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		GpuMemoryDomain::PersistentMapped,
		/*requestDeviceAddress=*/ true);

	if (!AnimBlendBuffer.IsValid())
	{
		LOG_ENG_ERROR("[SkinningPass] GpuAnimBuffer allocation failed");
		return false;
	}
	std::memset(AnimBlendBuffer.MappedPtr, 0, maxEntities * sizeof(GpuAnimEntry));

	auto code = ReadSPIRV_Skin(TNX_SHADER_DIR "/compute/skinning.spv");
	if (code.empty())
	{
		LOG_ENG_WARN("[SkinningPass] skinning.spv not found — GPU skinning disabled (M1 mode)");
		return true;
	}

	VkShaderModuleCreateInfo modCI{};
	modCI.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	modCI.codeSize = code.size() * sizeof(uint32_t);
	modCI.pCode    = code.data();

	VkShaderModule mod = VK_NULL_HANDLE;
	if (vkCreateShaderModule(device, &modCI, nullptr, &mod) != VK_SUCCESS)
	{
		LOG_ENG_ERROR("[SkinningPass] vkCreateShaderModule failed");
		return true;
	}

	VkPipelineShaderStageCreateInfo stageCI{};
	stageCI.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageCI.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
	stageCI.module = mod;
	stageCI.pName  = "main";

	VkComputePipelineCreateInfo pipeCI{};
	pipeCI.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipeCI.stage  = stageCI;
	pipeCI.layout = pipelineLayout;

	const VkResult result = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeCI, nullptr, &Pipeline);
	vkDestroyShaderModule(device, mod, nullptr);

	if (result != VK_SUCCESS)
	{
		LOG_ENG_WARN_F("[SkinningPass] vkCreateComputePipelines failed (%d) — GPU skinning disabled", result);
		Pipeline = VK_NULL_HANDLE;
		return true;
	}

	PipeLayout = pipelineLayout;

	LOG_ENG_INFO_F("[SkinningPass] Initialized (GpuAnimBuffer: %.1f MB, pipeline ready)",
	               static_cast<float>(maxEntities * sizeof(GpuAnimEntry)) / (1024.f * 1024.f));
	return true;
}

void SkinningPass::Destroy(VkDevice device)
{
	if (Pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(device, Pipeline, nullptr);
		Pipeline = VK_NULL_HANDLE;
	}
}

// -----------------------------------------------------------------------
// UploadAnimData — pack CAnimBase + CAnimLayer SoA into per-entity GpuAnimEntry.
// All float fields arrive as raw float; SimFloat-stored fields are converted
// by the caller via .ToFloat() before being passed here.
// -----------------------------------------------------------------------

void SkinningPass::UploadAnimData(const uint32_t* skeletalEntities, uint32_t count,
                                   const uint32_t* baseAnimID,   const uint32_t* blendspaceID,
                                   const float*    baseTimestamp, const float*    blendCoordX,
                                   const float*    blendCoordY,
                                   const uint32_t* fadeAnimID,   const float*    fadeTimestamp,
                                   const float*    fadeAlpha,
                                   const uint32_t* layer0AnimID, const float*    layer0Timestamp,
                                   const float*    layer0Alpha,  const uint32_t* layer0Config,
                                   const uint32_t* layer1AnimID, const float*    layer1Timestamp,
                                   const float*    layer1Alpha,  const uint32_t* layer1Config)
{
	if (count == 0 || !AnimBlendBuffer.MappedPtr) return;

	auto* dst = static_cast<GpuAnimEntry*>(AnimBlendBuffer.MappedPtr);
	for (uint32_t i = 0; i < count; ++i)
	{
		const uint32_t e = skeletalEntities[i];
		dst[i].baseAnimID    = baseAnimID[e];
		dst[i].blendspaceID  = blendspaceID[e];
		dst[i].baseTimestamp = baseTimestamp[e];
		dst[i].blendCoordX   = blendCoordX[e];
		dst[i].blendCoordY   = blendCoordY[e];
		dst[i].fadeAnimID    = fadeAnimID[e];
		dst[i].fadeTimestamp = fadeTimestamp[e];
		dst[i].fadeAlpha     = fadeAlpha[e];
		dst[i].layer0AnimID  = layer0AnimID[e];
		dst[i].layer0Timestamp = layer0Timestamp[e];
		dst[i].layer0Alpha   = layer0Alpha[e];
		dst[i].layer0Config  = layer0Config[e];
		dst[i].layer1AnimID  = layer1AnimID[e];
		dst[i].layer1Timestamp = layer1Timestamp[e];
		dst[i].layer1Alpha   = layer1Alpha[e];
		dst[i].layer1Config  = layer1Config[e];
	}
}

void SkinningPass::Dispatch(VkCommandBuffer cmd, uint64_t fdAddr, uint32_t skeletalEntityCount)
{
	if (Pipeline == VK_NULL_HANDLE || skeletalEntityCount == 0) return;

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, Pipeline);
	vkCmdPushConstants(cmd, PipeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint64_t), &fdAddr);
	vkCmdDispatch(cmd, 1, skeletalEntityCount, 1);
}