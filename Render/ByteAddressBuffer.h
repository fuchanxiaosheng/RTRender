#ifndef __BYTEADDRESSBUFFER_H_
#define __BYTEADDRESSBUFFER_H_

#include "Core.h"
#include "Buffer.h"
#include "DescriptorAllocationBlock.h"


class ByteAddressBuffer : public Buffer
{
public:
	ByteAddressBuffer(const std::wstring& name = L"");
	ByteAddressBuffer(const D3D12_RESOURCE_DESC& resDesc, size_t numElements, size_t elementSize, const std::wstring& name = L"");

	size_t GetBufferSize() const
	{
		return mBufferSize;
	}
	virtual void CreateViews(size_t numElements, size_t elementSize) override;
	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc = nullptr) const override
	{
		return mSRV.GetDescriptorHandle();
	}
	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc = nullptr) const override
	{
		return mUAV.GetDescriptorHandle();
	}

protected:

private:
	size_t mBufferSize;

	DescriptorAllocationBlock mSRV;
	DescriptorAllocationBlock mUAV;
};


#endif
