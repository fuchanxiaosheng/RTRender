#ifndef __GENERATEMIPSPSO_H_
#define __GENERATEMIPSPSO_H_

#include "Core.h"
#include "RootSignature.h"
#include "DescriptorAllocationBlock.h"


struct alignas(16) GenerateMipsCB
{
	uint32_t SrcMipLevel;
	uint32_t MipLevelNum;
	uint32_t SrcDimension;
	uint32_t IsSRGB;
	DirectX::XMFLOAT2 TexelSize;
};

namespace GenerateMips
{
	enum
	{
		GenerateMipsCB,
		SrcMip,
		OutMip,
		RootParameterNum
	};
}

class GenerateMipsPSO
{
public:
	GenerateMipsPSO();
	const RootSignature& GetRootSignature() const
	{
		return mRootSignature;
	}
	ComPtr<ID3D12PipelineState> GetPipelineState() const
	{
		return mPipelineState;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GetDefaultUAV() const
	{
		return mDefaultUAV.GetDescriptorHandle();
	}

private:
	RootSignature mRootSignature;
	ComPtr<ID3D12PipelineState> mPipelineState;
	DescriptorAllocationBlock mDefaultUAV;
};

#endif
