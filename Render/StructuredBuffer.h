#ifndef __STRUCTUREDBUFFER_H_
#define __STRUCTUREDBUFFER_H_

#include "Core.h"
#include "Buffer.h"
#include "ByteAddressBuffer.h"



class StructuredBuffer : public Buffer
{
public:
	StructuredBuffer(const std::wstring& name = L"");
	StructuredBuffer(const D3D12_RESOURCE_DESC& resDesc, size_t numElements, size_t elementSize, const std::wstring& name = L"");

	virtual size_t GetNumElements() const
	{
		return mElementNum;
	}

	virtual size_t GetElementSize() const
	{
		return mElementSize;
	}

	virtual void CreateViews(size_t numElements, size_t elementSize) override;

	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc = nullptr) const
	{
		return mSRV.GetDescriptorHandle();
	}

	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc = nullptr) const
	{
		return mUAV.GetDescriptorHandle();
	}

	const ByteAddressBuffer& GetCounterBuffer() const
	{
		return mCounterBuffer;
	}

private:
	size_t mElementNum;
	size_t mElementSize;

	DescriptorAllocationBlock mSRV;
	DescriptorAllocationBlock mUAV;

	ByteAddressBuffer mCounterBuffer;
};

#endif
