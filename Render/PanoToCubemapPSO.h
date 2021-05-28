#ifndef __PANOTOCUBEMAPPSO_H_
#define __PANOTOCUBEMAPPSO_H_

#include "Core.h"
#include "RootSignature.h"
#include "DescriptorAllocationBlock.h"

struct PanoToCubemapCB
{
	uint32_t CubemapSize;
	uint32_t FirstMip;
	uint32_t NumMips;
};

namespace PanoToCubemapRS
{
	enum
	{
		PanoToCubemapCB,
		SrcTexture,
		DstMips,
		NumRootParameters
	};
}

class PanoToCubemapPSO
{
public:
	PanoToCubemapPSO();

	const RootSignature& GetRootSignature() const
	{
		return mRootSignature;
	}

	Microsoft::WRL::ComPtr<ID3D12PipelineState> GetPipelineState() const
	{
		return mPipelineState;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GetDefaultUAV() const
	{
		return mDefaultUAV.GetDescriptorHandle();
	}

private:
	RootSignature mRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mPipelineState;
	DescriptorAllocationBlock mDefaultUAV;
};

#endif
