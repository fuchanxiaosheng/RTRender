#ifndef __VERTEXBUFFER_H_
#define __VERTEXBUFFER_H_

#include "Core.h"
#include "Buffer.h"

class VertexBuffer : public Buffer
{
public:
	VertexBuffer(const std::wstring& name = L"");
	virtual ~VertexBuffer();

	virtual void CreateViews(size_t numElements, size_t elementSize) override;

	D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const
	{
		return mVertexBufferView;
	}

	size_t GetNumVertices() const
	{
		return mVerticesNum;
	}

	size_t GetVertexStride() const
	{
		return mVertexStride;
	}

	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc = nullptr) const override;

	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc = nullptr) const override;

protected:

private:
	size_t mVerticesNum;
	size_t mVertexStride;

	D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;
};

#endif
