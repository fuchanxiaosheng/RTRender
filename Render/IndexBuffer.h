#ifndef __INDEXBUFFER_H_
#define __INDEXBUFFER_H_

#include "Core.h"
#include "Buffer.h"

class IndexBuffer : public Buffer
{
public:
	IndexBuffer(const std::wstring& name = L"");
	virtual ~IndexBuffer();

	virtual void CreateViews(size_t numElements, size_t elementSize) override;

	size_t GetNumIndicies() const
	{
		return mIndiciesNum;
	}

	DXGI_FORMAT GetIndexFormat() const
	{
		return mIndexFormat;
	}
	D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() const
	{
		return mIndexBufferView;
	}
	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc = nullptr) const override;

	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc = nullptr) const override;

protected:

private:
	size_t mIndiciesNum;
	DXGI_FORMAT mIndexFormat;

	D3D12_INDEX_BUFFER_VIEW mIndexBufferView;
};

#endif
