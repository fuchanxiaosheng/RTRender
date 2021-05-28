#ifndef __ROOTSIGNATURE_H_
#define __ROOTSIGNATURE_H_

#include "Core.h"

class RootSignature
{
public:
	RootSignature();
	RootSignature(const D3D12_ROOT_SIGNATURE_DESC1& rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION rootSignatureVersion);
	virtual ~RootSignature();
	void Destroy();
	ComPtr<ID3D12RootSignature> GetRootSignature() const
	{
		return mRootSignature;
	}
	void SetRootSignatureDesc(const D3D12_ROOT_SIGNATURE_DESC1& rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION rootSignatureVersion);
	const D3D12_ROOT_SIGNATURE_DESC1& GetRootSignatureDesc() const
	{
		return mRootSignatureDesc;
	}
	uint32_t GetDescriptorTableBitMask(D3D12_DESCRIPTOR_HEAP_TYPE descriptorHeapType) const;
	uint32_t GetNumDescriptors(uint32_t rootIndex) const;

	
private:
	D3D12_ROOT_SIGNATURE_DESC1 mRootSignatureDesc;
	ComPtr<ID3D12RootSignature> mRootSignature;

	uint32_t mDescriptorNumPerTable[32];
	uint32_t mSamplerTableBitMask;
	uint32_t mDescriptorTableBitMask;
};

#endif
