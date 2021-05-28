#include "StructuredBuffer.h"
#include "Application.h"
#include "ResourceStateTracker.h"


StructuredBuffer::StructuredBuffer(const std::wstring& name)
	: Buffer(name), mCounterBuffer(CD3DX12_RESOURCE_DESC::Buffer(4, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS), 1, 4, name + L" Counter"), mElementNum(0), mElementSize(0)
{
	mSRV = Application::Get().AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	mUAV = Application::Get().AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

StructuredBuffer::StructuredBuffer(const D3D12_RESOURCE_DESC& resDesc,size_t numElements, size_t elementSize, const std::wstring& name)
	: Buffer(resDesc, numElements, elementSize, name), mCounterBuffer(CD3DX12_RESOURCE_DESC::Buffer(4, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS), 1, 4, name + L" Counter")
	, mElementNum(numElements), mElementSize(elementSize)
{
	mSRV = Application::Get().AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	mUAV = Application::Get().AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void StructuredBuffer::CreateViews(size_t numElements, size_t elementSize)
{
	auto device = Application::Get().GetDevice();

	mElementNum = numElements;
	mElementSize = elementSize;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.NumElements = static_cast<UINT>(mElementNum);
	srvDesc.Buffer.StructureByteStride = static_cast<UINT>(mElementSize);
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	device->CreateShaderResourceView(mResource.Get(), &srvDesc, mSRV.GetDescriptorHandle());

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.NumElements = static_cast<UINT>(mElementNum);
	uavDesc.Buffer.StructureByteStride = static_cast<UINT>(mElementSize);
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	device->CreateUnorderedAccessView(mResource.Get(), mCounterBuffer.GetD3D12Resource().Get(), &uavDesc, mUAV.GetDescriptorHandle());
}
