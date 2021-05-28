#include "ConstantBuffer.h"
#include "Application.h"


ConstantBuffer::ConstantBuffer(const std::wstring& name) : Buffer(name), mSizeInBytes(0)
{
	mConstantBufferView = Application::Get().AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

ConstantBuffer::~ConstantBuffer()
{}

void ConstantBuffer::CreateViews(size_t numElements, size_t elementSize)
{
	mSizeInBytes = numElements * elementSize;

	D3D12_CONSTANT_BUFFER_VIEW_DESC d3d12ConstantBufferViewDesc;
	d3d12ConstantBufferViewDesc.BufferLocation = mResource->GetGPUVirtualAddress();
	d3d12ConstantBufferViewDesc.SizeInBytes = static_cast<UINT>(Math::AlignUp(mSizeInBytes, 16));

	auto device = Application::Get().GetDevice();

	device->CreateConstantBufferView(&d3d12ConstantBufferViewDesc, mConstantBufferView.GetDescriptorHandle());
}

D3D12_CPU_DESCRIPTOR_HANDLE ConstantBuffer::GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc) const
{
	throw std::exception("别用ConstantBuffer::GetShaderResourceView.");
}

D3D12_CPU_DESCRIPTOR_HANDLE ConstantBuffer::GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc) const
{
	throw std::exception("别用ConstantBuffer::GetUnorderedAccessView");
}