#include "VertexBuffer.h"

VertexBuffer::VertexBuffer(const std::wstring& name)
	: Buffer(name)
	, mVerticesNum(0)
	, mVertexStride(0)
	, mVertexBufferView({})
{}

VertexBuffer::~VertexBuffer()
{}

void VertexBuffer::CreateViews(size_t numElements, size_t elementSize)
{
	mVerticesNum = numElements;
	mVertexStride = elementSize;

	mVertexBufferView.BufferLocation = mResource->GetGPUVirtualAddress();
	mVertexBufferView.SizeInBytes = static_cast<UINT>(mVerticesNum * mVertexStride);
	mVertexBufferView.StrideInBytes = static_cast<UINT>(mVertexStride);
}

D3D12_CPU_DESCRIPTOR_HANDLE VertexBuffer::GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc) const
{
	throw std::exception("别调用VertexBuffer::GetShaderResourceView");
}

D3D12_CPU_DESCRIPTOR_HANDLE VertexBuffer::GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc) const
{
	throw std::exception("别调用VertexBuffer::GetUnorderedAccessView");
}
