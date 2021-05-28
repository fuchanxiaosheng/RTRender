#include "IndexBuffer.h"

IndexBuffer::IndexBuffer(const std::wstring& name) : Buffer(name), mIndiciesNum(0), mIndexFormat(DXGI_FORMAT_UNKNOWN), mIndexBufferView({}) {}

IndexBuffer::~IndexBuffer() {}

void IndexBuffer::CreateViews(size_t numElements, size_t elementSize)
{
	assert(elementSize == 2 || elementSize == 4 && "索引必须为16或32位整数");

	mIndiciesNum = numElements;
	mIndexFormat = (elementSize == 2) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

	mIndexBufferView.BufferLocation = mResource->GetGPUVirtualAddress();
	mIndexBufferView.SizeInBytes = static_cast<UINT>(numElements * elementSize);
	mIndexBufferView.Format = mIndexFormat;
}

D3D12_CPU_DESCRIPTOR_HANDLE IndexBuffer::GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc) const
{
	throw std::exception("别调用IndexBuffer::GetShaderResourceView");
}

D3D12_CPU_DESCRIPTOR_HANDLE IndexBuffer::GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc) const
{
	throw std::exception("别调用IndexBuffer::GetUnorderedAccessView");
}
