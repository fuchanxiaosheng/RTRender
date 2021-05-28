#include "RootSignature.h"
#include "Application.h"

RootSignature::RootSignature(): mRootSignatureDesc{}, mDescriptorNumPerTable{ 0 }, mSamplerTableBitMask(0), mDescriptorTableBitMask(0){}

RootSignature::RootSignature(const D3D12_ROOT_SIGNATURE_DESC1& rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION rootSignatureVersion)
	: mRootSignatureDesc{}, mDescriptorNumPerTable{ 0 }, mSamplerTableBitMask(0), mDescriptorTableBitMask(0)
{
	SetRootSignatureDesc(rootSignatureDesc, rootSignatureVersion);
}


RootSignature::~RootSignature()
{
	Destroy();
}

void RootSignature::Destroy()
{
	for (UINT i = 0; i < mRootSignatureDesc.NumParameters; ++i)
	{
		const D3D12_ROOT_PARAMETER1& rootParameter = mRootSignatureDesc.pParameters[i];
		if (rootParameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
		{
			delete[] rootParameter.DescriptorTable.pDescriptorRanges;
		}
	}

	delete[] mRootSignatureDesc.pParameters;
	mRootSignatureDesc.pParameters = nullptr;
	mRootSignatureDesc.NumParameters = 0;

	delete[] mRootSignatureDesc.pStaticSamplers;
	mRootSignatureDesc.pStaticSamplers = nullptr;
	mRootSignatureDesc.NumStaticSamplers = 0;

	mDescriptorTableBitMask = 0;
	mSamplerTableBitMask = 0;

	memset(mDescriptorNumPerTable, 0, sizeof(mDescriptorNumPerTable));
}

void RootSignature::SetRootSignatureDesc(const D3D12_ROOT_SIGNATURE_DESC1& rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION rootSignatureVersion)
{
	Destroy();
	auto device = Application::Get().GetDevice();
	UINT numParameters = rootSignatureDesc.NumParameters;
	D3D12_ROOT_PARAMETER1* pParameters = numParameters > 0 ? new D3D12_ROOT_PARAMETER1[numParameters] : nullptr;

	for (UINT i = 0; i < numParameters; ++i)
	{
		const D3D12_ROOT_PARAMETER1& rootParameter = rootSignatureDesc.pParameters[i];
		pParameters[i] = rootParameter;
		if (rootParameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
		{
			UINT numDescriptorRanges = rootParameter.DescriptorTable.NumDescriptorRanges;
			D3D12_DESCRIPTOR_RANGE1* pDescriptorRanges = numDescriptorRanges > 0 ? new D3D12_DESCRIPTOR_RANGE1[numDescriptorRanges] : nullptr;
			memcpy(pDescriptorRanges, rootParameter.DescriptorTable.pDescriptorRanges, sizeof(D3D12_DESCRIPTOR_RANGE1) * numDescriptorRanges);

			pParameters[i].DescriptorTable.NumDescriptorRanges = numDescriptorRanges;
			pParameters[i].DescriptorTable.pDescriptorRanges = pDescriptorRanges;
			if (numDescriptorRanges > 0)
			{
				switch (pDescriptorRanges[0].RangeType)
				{
				case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
				case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
				case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
					mDescriptorTableBitMask |= (1 << i);
					break;
				case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:
					mSamplerTableBitMask |= (1 << i);
					break;
				}
			}
			for (UINT j = 0; j < numDescriptorRanges; ++j)
			{
				mDescriptorNumPerTable[i] += pDescriptorRanges[j].NumDescriptors;
			}
		}
	}

	mRootSignatureDesc.NumParameters = numParameters;
	mRootSignatureDesc.pParameters = pParameters;

	UINT numStaticSamplers = rootSignatureDesc.NumStaticSamplers;
	D3D12_STATIC_SAMPLER_DESC* pStaticSamplers = numStaticSamplers > 0 ? new D3D12_STATIC_SAMPLER_DESC[numStaticSamplers] : nullptr;

	if (pStaticSamplers)
	{
		memcpy(pStaticSamplers, rootSignatureDesc.pStaticSamplers,sizeof(D3D12_STATIC_SAMPLER_DESC) * numStaticSamplers);
	}

	mRootSignatureDesc.NumStaticSamplers = numStaticSamplers;
	mRootSignatureDesc.pStaticSamplers = pStaticSamplers;

	D3D12_ROOT_SIGNATURE_FLAGS flags = rootSignatureDesc.Flags;
	mRootSignatureDesc.Flags = flags;

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC versionRootSignatureDesc;
	versionRootSignatureDesc.Init_1_1(numParameters, pParameters, numStaticSamplers, pStaticSamplers, flags);

	ComPtr<ID3DBlob> rootSignatureBlob;
	ComPtr<ID3DBlob> errorBlob;
	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&versionRootSignatureDesc, rootSignatureVersion, &rootSignatureBlob, &errorBlob));

	ThrowIfFailed(device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));
}

uint32_t RootSignature::GetDescriptorTableBitMask(D3D12_DESCRIPTOR_HEAP_TYPE descriptorHeapType) const
{
	uint32_t descriptorTableBitMask = 0;
	switch (descriptorHeapType)
	{
	case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
		descriptorTableBitMask = mDescriptorTableBitMask;
		break;
	case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:
		descriptorTableBitMask = mSamplerTableBitMask;
		break;
	}

	return descriptorTableBitMask;
}

uint32_t RootSignature::GetNumDescriptors(uint32_t rootIndex) const
{
	assert(rootIndex < 32);
	return mDescriptorNumPerTable[rootIndex];
}