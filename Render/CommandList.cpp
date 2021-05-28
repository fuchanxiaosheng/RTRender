#include "CommandList.h"

#include "Application.h"
#include "ByteAddressBuffer.h"
#include "ConstantBuffer.h"
#include "CommandQueue.h"
#include "DynamicDescriptorHeap.h"
#include "GenerateMipsPSO.h"
#include "IndexBuffer.h"
#include "PanoToCubemapPSO.h"
#include "RenderTarget.h"
#include "Resource.h"
#include "ResourceStateTracker.h"
#include "RootSignature.h"
#include "StructuredBuffer.h"
#include "Texture.h"
#include "UploadBuffer.h"
#include "VertexBuffer.h"


std::map<std::wstring, ID3D12Resource* > CommandList::msTextureCache;
std::mutex CommandList::msTextureCacheMutex;

CommandList::CommandList(D3D12_COMMAND_LIST_TYPE type) : mCommandListType(type)
{
	auto device = Application::Get().GetDevice();

	ThrowIfFailed(device->CreateCommandAllocator(mCommandListType, IID_PPV_ARGS(&mCommandAllocator)));
	ThrowIfFailed(device->CreateCommandList(0, mCommandListType, mCommandAllocator.Get(), nullptr, IID_PPV_ARGS(&mCommandList)));

	mUploadBuffer = std::make_unique<UploadBuffer>();

	mResourceStateTracker = std::make_unique<ResourceStateTracker>();

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		mDynamicDescriptorHeap[i] = std::make_unique<DynamicDescriptorHeap>(static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(i));
		mDescriptorHeaps[i] = nullptr;
	}
}

CommandList::~CommandList() {}

void CommandList::TransitionBarrier(ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES stateAfter, UINT subresource, bool flushBarriers)
{
	if (resource)
	{
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), D3D12_RESOURCE_STATE_COMMON, stateAfter, subresource);
		mResourceStateTracker->ResourceBarrier(barrier);
	}

	if (flushBarriers)
	{
		FlushResourceBarriers();
	}
}

void CommandList::TransitionBarrier(const Resource& resource, D3D12_RESOURCE_STATES stateAfter, UINT subresource, bool flushBarriers)
{
	TransitionBarrier(resource.GetD3D12Resource(), stateAfter, subresource, flushBarriers);
}

void CommandList::UAVBarrier(ComPtr<ID3D12Resource> resource, bool flushBarriers)
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(resource.Get());

	mResourceStateTracker->ResourceBarrier(barrier);

	if (flushBarriers)
	{
		FlushResourceBarriers();
	}
}

void CommandList::UAVBarrier(const Resource& resource, bool flushBarriers)
{
	UAVBarrier(resource.GetD3D12Resource());
}

void CommandList::AliasingBarrier(ComPtr<ID3D12Resource> beforeResource, ComPtr<ID3D12Resource> afterResource, bool flushBarriers)
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Aliasing(beforeResource.Get(), afterResource.Get());

	mResourceStateTracker->ResourceBarrier(barrier);

	if (flushBarriers)
	{
		FlushResourceBarriers();
	}
}

void CommandList::AliasingBarrier(const Resource& beforeResource, const Resource& afterResource, bool flushBarriers)
{
	AliasingBarrier(beforeResource.GetD3D12Resource(), afterResource.GetD3D12Resource());
}

void CommandList::FlushResourceBarriers()
{
	mResourceStateTracker->FlushResourceBarriers(*this);
}

void CommandList::CopyResource(ComPtr<ID3D12Resource> dstRes, ComPtr<ID3D12Resource> srcRes)
{
	TransitionBarrier(dstRes, D3D12_RESOURCE_STATE_COPY_DEST);
	TransitionBarrier(srcRes, D3D12_RESOURCE_STATE_COPY_SOURCE);

	FlushResourceBarriers();

	mCommandList->CopyResource(dstRes.Get(), srcRes.Get());

	TrackResource(dstRes);
	TrackResource(srcRes);
}

void CommandList::CopyResource(Resource& dstRes, const Resource& srcRes)
{
	CopyResource(dstRes.GetD3D12Resource(), srcRes.GetD3D12Resource());
}

void CommandList::ResolveSubresource(Resource& dstRes, const Resource& srcRes, uint32_t dstSubresource, uint32_t srcSubresource)
{
	TransitionBarrier(dstRes, D3D12_RESOURCE_STATE_RESOLVE_DEST, dstSubresource);
	TransitionBarrier(srcRes, D3D12_RESOURCE_STATE_RESOLVE_SOURCE, srcSubresource);

	FlushResourceBarriers();

	mCommandList->ResolveSubresource(dstRes.GetD3D12Resource().Get(), dstSubresource, srcRes.GetD3D12Resource().Get(), srcSubresource, dstRes.GetD3D12ResourceDesc().Format);

	TrackResource(srcRes);
	TrackResource(dstRes);
}


void CommandList::CopyBuffer(Buffer& buffer, size_t numElements, size_t elementSize, const void* bufferData, D3D12_RESOURCE_FLAGS flags)
{
	auto device = Application::Get().GetDevice();

	size_t bufferSize = numElements * elementSize;

	ComPtr<ID3D12Resource> resource;
	if (bufferSize == 0)
	{
	}
	else
	{
		ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags),
													  D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&resource)));
		ResourceStateTracker::AddGlobalResourceState(resource.Get(), D3D12_RESOURCE_STATE_COMMON);
		if (bufferData != nullptr)
		{
			ComPtr<ID3D12Resource> uploadResource;
			ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
														  D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadResource)));
			D3D12_SUBRESOURCE_DATA subresourceData = {};
			subresourceData.pData = bufferData;
			subresourceData.RowPitch = bufferSize;
			subresourceData.SlicePitch = subresourceData.RowPitch;

			mResourceStateTracker->TransitionResource(resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST);
			FlushResourceBarriers();

			UpdateSubresources(mCommandList.Get(), resource.Get(), uploadResource.Get(), 0, 0, 1, &subresourceData);
			TrackResource(uploadResource);
		}
		TrackResource(resource);
	}

	buffer.SetResource(resource);
	buffer.CreateViews(numElements, elementSize);
}

void CommandList::CopyVertexBuffer(VertexBuffer& vertexBuffer, size_t numVertices, size_t vertexStride, const void* vertexBufferData)
{
	CopyBuffer(vertexBuffer, numVertices, vertexStride, vertexBufferData);
}

void CommandList::CopyIndexBuffer(IndexBuffer& indexBuffer, size_t numIndicies, DXGI_FORMAT indexFormat, const void* indexBufferData)
{
	size_t indexSizeInBytes = indexFormat == DXGI_FORMAT_R16_UINT ? 2 : 4;
	CopyBuffer(indexBuffer, numIndicies, indexSizeInBytes, indexBufferData);
}

void CommandList::CopyByteAddressBuffer(ByteAddressBuffer& byteAddressBuffer, size_t bufferSize, const void* bufferData)
{
	CopyBuffer(byteAddressBuffer, 1, bufferSize, bufferData, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
}

void CommandList::CopyStructuredBuffer(StructuredBuffer& structuredBuffer, size_t numElements, size_t elementSize, const void* bufferData)
{
	CopyBuffer(structuredBuffer, numElements, elementSize, bufferData, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
}

void CommandList::SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY primitiveTopology)
{
	mCommandList->IASetPrimitiveTopology(primitiveTopology);
}

void CommandList::LoadTextureFromFile(Texture& texture, const std::wstring& fileName, TextureUsage textureUsage)
{
	fs::path filePath(fileName);
	if (!fs::exists(filePath))
	{
		throw std::exception("File not found.");
	}

	std::lock_guard<std::mutex> lock(msTextureCacheMutex);
	auto iter = msTextureCache.find(fileName);
	if (iter != msTextureCache.end())
	{
		texture.SetTextureUsage(textureUsage);
		texture.SetResource(iter->second);
		texture.CreateViews();
		texture.SetName(fileName);
	}
	else
	{
		TexMetadata metadata;
		ScratchImage scratchImage;

		if (filePath.extension() == ".dds")
		{
			ThrowIfFailed(LoadFromDDSFile(fileName.c_str(), DDS_FLAGS_NONE, &metadata, scratchImage));
		}
		else if (filePath.extension() == ".hdr")
		{
			ThrowIfFailed(LoadFromHDRFile(fileName.c_str(), &metadata, scratchImage));
		}
		else if (filePath.extension() == ".tga")
		{
			ThrowIfFailed(LoadFromTGAFile(fileName.c_str(), &metadata, scratchImage));
		}
		else
		{
			ThrowIfFailed(LoadFromWICFile(fileName.c_str(), WIC_FLAGS_NONE, &metadata, scratchImage));
		}

		D3D12_RESOURCE_DESC textureDesc = {};
		switch (metadata.dimension)
		{
		case TEX_DIMENSION_TEXTURE1D:
			textureDesc = CD3DX12_RESOURCE_DESC::Tex1D(metadata.format, static_cast<UINT64>(metadata.width), static_cast<UINT16>(metadata.arraySize));
			break;
		case TEX_DIMENSION_TEXTURE2D:
			textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(metadata.format, static_cast<UINT64>(metadata.width), static_cast<UINT>(metadata.height), static_cast<UINT16>(metadata.arraySize));
			break;
		case TEX_DIMENSION_TEXTURE3D:
			textureDesc = CD3DX12_RESOURCE_DESC::Tex3D(metadata.format, static_cast<UINT64>(metadata.width), static_cast<UINT>(metadata.height), static_cast<UINT16>(metadata.depth));
			break;
		default:
			throw std::exception("无效的TextureDimension.");
			break;
		}

		auto device = Application::Get().GetDevice();
		Microsoft::WRL::ComPtr<ID3D12Resource> textureResource;

		ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&textureResource)));
		texture.SetTextureUsage(textureUsage);
		texture.SetResource(textureResource);
		texture.CreateViews();
		texture.SetName(fileName);

		ResourceStateTracker::AddGlobalResourceState(textureResource.Get(), D3D12_RESOURCE_STATE_COMMON);

		std::vector<D3D12_SUBRESOURCE_DATA> subresources(scratchImage.GetImageCount());
		const Image* pImages = scratchImage.GetImages();
		for (int i = 0; i < scratchImage.GetImageCount(); ++i)
		{
			auto& subresource = subresources[i];
			subresource.RowPitch = pImages[i].rowPitch;
			subresource.SlicePitch = pImages[i].slicePitch;
			subresource.pData = pImages[i].pixels;
		}

		CopyTextureSubresource(texture, 0, static_cast<uint32_t>(subresources.size()), subresources.data());
		if (subresources.size() < textureResource->GetDesc().MipLevels)
		{
			GenerateMips(texture);
		}

		msTextureCache[fileName] = textureResource.Get();
	}
}

void CommandList::GenerateMips(Texture& texture)
{
	if (mCommandListType == D3D12_COMMAND_LIST_TYPE_COPY)
	{
		if (!mComputeCommandList)
		{
			mComputeCommandList = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE)->GetCommandList();
		}
		mComputeCommandList->GenerateMips(texture);
		return;
	}

	auto resource = texture.GetD3D12Resource();
	if (!resource) return;
	auto resourceDesc = resource->GetDesc();
	if (resourceDesc.MipLevels == 1) return;
	if (resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
		resourceDesc.DepthOrArraySize != 1 ||
		resourceDesc.SampleDesc.Count > 1)
	{
		throw std::exception("GenerateMips只支持无多重采样的2D纹理");
	}

	ComPtr<ID3D12Resource> uavResource = resource;
	ComPtr<ID3D12Resource> aliasResource;
	if (!texture.CheckUAVSupport() || (resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == 0)
	{
		auto device = Application::Get().GetDevice();
		auto aliasDesc = resourceDesc;
		aliasDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		aliasDesc.Flags &= ~(D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
		auto uavDesc = aliasDesc;   
		uavDesc.Format = Texture::GetUAVCompatableFormat(resourceDesc.Format);

		D3D12_RESOURCE_DESC resourceDescs[] = {
			aliasDesc,
			uavDesc
		};
		auto allocationInfo = device->GetResourceAllocationInfo(0, _countof(resourceDescs), resourceDescs);

		D3D12_HEAP_DESC heapDesc = {};
		heapDesc.SizeInBytes = allocationInfo.SizeInBytes;
		heapDesc.Alignment = allocationInfo.Alignment;
		heapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;
		heapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;

		ComPtr<ID3D12Heap> heap;
		ThrowIfFailed(device->CreateHeap(&heapDesc, IID_PPV_ARGS(&heap)));
		TrackResource(heap);
		ThrowIfFailed(device->CreatePlacedResource(heap.Get(), 0, &aliasDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&aliasResource)));

		ResourceStateTracker::AddGlobalResourceState(aliasResource.Get(), D3D12_RESOURCE_STATE_COMMON);
		TrackResource(aliasResource);
		ThrowIfFailed(device->CreatePlacedResource(heap.Get(), 0, &uavDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&uavResource)));

		ResourceStateTracker::AddGlobalResourceState(uavResource.Get(), D3D12_RESOURCE_STATE_COMMON);
		TrackResource(uavResource);

		AliasingBarrier(nullptr, aliasResource);
		CopyResource(aliasResource, resource);
		AliasingBarrier(aliasResource, uavResource);
	}
	Texture tempT = Texture(uavResource, texture.GetTextureUsage());
	GenerateMips_UAV(tempT, resourceDesc.Format);

	if (aliasResource)
	{
		AliasingBarrier(uavResource, aliasResource);
		CopyResource(resource, aliasResource);
	}
}

void CommandList::GenerateMips_UAV(Texture& texture, DXGI_FORMAT format)
{
	if (!mGenerateMipsPSO)
	{
		mGenerateMipsPSO = std::make_unique<GenerateMipsPSO>();
	}

	mCommandList->SetPipelineState(mGenerateMipsPSO->GetPipelineState().Get());
	SetComputeRootSignature(mGenerateMipsPSO->GetRootSignature());

	GenerateMipsCB generateMipsCB;
	generateMipsCB.IsSRGB = Texture::IsSRGBFormat(format);

	auto resource = texture.GetD3D12Resource();
	auto resourceDesc = resource->GetDesc();


	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; 
	srvDesc.Texture2D.MipLevels = resourceDesc.MipLevels;

	for (uint32_t srcMip = 0; srcMip < resourceDesc.MipLevels - 1u; )
	{
		uint64_t srcWidth = resourceDesc.Width >> srcMip;
		uint32_t srcHeight = resourceDesc.Height >> srcMip;
		uint32_t dstWidth = static_cast<uint32_t>(srcWidth >> 1);
		uint32_t dstHeight = srcHeight >> 1;
		generateMipsCB.SrcDimension = (srcHeight & 1) << 1 | (srcWidth & 1);

		DWORD mipCount;
		_BitScanForward(&mipCount, (dstWidth == 1 ? dstHeight : dstWidth) | (dstHeight == 1 ? dstWidth : dstHeight));
		mipCount = std::min<DWORD>(4, mipCount + 1);
		mipCount = (srcMip + mipCount) >= resourceDesc.MipLevels ? resourceDesc.MipLevels - srcMip - 1 : mipCount;

		dstWidth = std::max<DWORD>(1, dstWidth);
		dstHeight = std::max<DWORD>(1, dstHeight);

		generateMipsCB.SrcMipLevel = srcMip;
		generateMipsCB.MipLevelNum = mipCount;
		generateMipsCB.TexelSize.x = 1.0f / (float)dstWidth;
		generateMipsCB.TexelSize.y = 1.0f / (float)dstHeight;

		SetCompute32BitConstants(GenerateMips::GenerateMipsCB, generateMipsCB);
		SetShaderResourceView(GenerateMips::SrcMip, 0, texture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, srcMip, 1, &srvDesc);

		for (uint32_t mip = 0; mip < mipCount; ++mip)
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
			uavDesc.Format = resourceDesc.Format;
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			uavDesc.Texture2D.MipSlice = srcMip + mip + 1;

			SetUnorderedAccessView(GenerateMips::OutMip, mip, texture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, srcMip + mip + 1, 1, &uavDesc);
		}

		if (mipCount < 4)
		{
			mDynamicDescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->CachingDescriptors(GenerateMips::OutMip, mipCount, 4 - mipCount, mGenerateMipsPSO->GetDefaultUAV());
		}

		Dispatch(Math::DivideByMultiple(dstWidth, 8), Math::DivideByMultiple(dstHeight, 8));

		UAVBarrier(texture);

		srcMip += mipCount;
	}
}

void CommandList::PanoToCubemap(Texture& cubemapTexture, const Texture& panoTexture)
{
	if (mCommandListType == D3D12_COMMAND_LIST_TYPE_COPY)
	{
		if (!mComputeCommandList)
		{
			mComputeCommandList = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE)->GetCommandList();
		}
		mComputeCommandList->PanoToCubemap(cubemapTexture, panoTexture);
		return;
	}

	if (!mPanoToCubemapPSO)
	{
		mPanoToCubemapPSO = std::make_unique<PanoToCubemapPSO>();
	}

	auto device = Application::Get().GetDevice();

	auto cubemapResource = cubemapTexture.GetD3D12Resource();
	if (!cubemapResource) return;

	CD3DX12_RESOURCE_DESC cubemapDesc(cubemapResource->GetDesc());

	auto stagingResource = cubemapResource;
	Texture stagingTexture(stagingResource);
	if ((cubemapDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == 0)
	{
		auto stagingDesc = cubemapDesc;
		stagingDesc.Format = Texture::GetUAVCompatableFormat(cubemapDesc.Format);
		stagingDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &stagingDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&stagingResource)));

		ResourceStateTracker::AddGlobalResourceState(stagingResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST);

		stagingTexture.SetResource(stagingResource);
		stagingTexture.CreateViews();
		stagingTexture.SetName(L"PanoToCubemapStagingTexture");
		CopyResource(stagingTexture, cubemapTexture);
	}

	TransitionBarrier(stagingTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	mCommandList->SetPipelineState(mPanoToCubemapPSO->GetPipelineState().Get());
	SetComputeRootSignature(mPanoToCubemapPSO->GetRootSignature());
	PanoToCubemapCB panoToCubemapCB;
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = Texture::GetUAVCompatableFormat(cubemapDesc.Format);
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
	uavDesc.Texture2DArray.FirstArraySlice = 0;
	uavDesc.Texture2DArray.ArraySize = 6;

	for (uint32_t mipSlice = 0; mipSlice < cubemapDesc.MipLevels; )
	{
		uint32_t numMips = std::min<uint32_t>(5, cubemapDesc.MipLevels - mipSlice);

		panoToCubemapCB.FirstMip = mipSlice;
		panoToCubemapCB.CubemapSize = std::max<uint32_t>(static_cast<uint32_t>(cubemapDesc.Width), cubemapDesc.Height) >> mipSlice;
		panoToCubemapCB.NumMips = numMips;

		SetCompute32BitConstants(PanoToCubemapRS::PanoToCubemapCB, panoToCubemapCB);

		SetShaderResourceView(PanoToCubemapRS::SrcTexture, 0, panoTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		for (uint32_t mip = 0; mip < numMips; ++mip)
		{
			uavDesc.Texture2DArray.MipSlice = mipSlice + mip;
			SetUnorderedAccessView(PanoToCubemapRS::DstMips, mip, stagingTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0, 0, &uavDesc);
		}

		if (numMips < 5)
		{
			mDynamicDescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->CachingDescriptors(PanoToCubemapRS::DstMips, panoToCubemapCB.NumMips, 5 - numMips, mPanoToCubemapPSO->GetDefaultUAV());
		}

		Dispatch(Math::DivideByMultiple(panoToCubemapCB.CubemapSize, 16), Math::DivideByMultiple(panoToCubemapCB.CubemapSize, 16), 6);

		mipSlice += numMips;
	}

	if (stagingResource != cubemapResource)
	{
		CopyResource(cubemapTexture, stagingTexture);
	}
}

void CommandList::ClearTexture(const Texture& texture, const float clearColor[4])
{
	TransitionBarrier(texture, D3D12_RESOURCE_STATE_RENDER_TARGET);
	mCommandList->ClearRenderTargetView(texture.GetRenderTargetView(), clearColor, 0, nullptr);

	TrackResource(texture);
}

void CommandList::ClearDepthStencilTexture(const Texture& texture, D3D12_CLEAR_FLAGS clearFlags, float depth, uint8_t stencil)
{
	TransitionBarrier(texture, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	mCommandList->ClearDepthStencilView(texture.GetDepthStencilView(), clearFlags, depth, stencil, 0, nullptr);

	TrackResource(texture);
}

void CommandList::CopyTextureSubresource(Texture& texture, uint32_t firstSubresource, uint32_t numSubresources, D3D12_SUBRESOURCE_DATA* subresourceData)
{
	auto device = Application::Get().GetDevice();
	auto destinationResource = texture.GetD3D12Resource();

	if (destinationResource)
	{
		TransitionBarrier(texture, D3D12_RESOURCE_STATE_COPY_DEST);
		FlushResourceBarriers();

		UINT64 requiredSize = GetRequiredIntermediateSize(destinationResource.Get(), firstSubresource, numSubresources);
		ComPtr<ID3D12Resource> intermediateResource;
		ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(requiredSize), 
													  D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&intermediateResource)));

		UpdateSubresources(mCommandList.Get(), destinationResource.Get(), intermediateResource.Get(), 0, firstSubresource, numSubresources, subresourceData);

		TrackResource(intermediateResource);
		TrackResource(destinationResource);
	}
}

void CommandList::SetGraphicsDynamicConstantBuffer(uint32_t rootParameterIndex, size_t sizeInBytes, const void* bufferData)
{
	auto heapAllococation = mUploadBuffer->Allocate(sizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	memcpy(heapAllococation.CPUAddress, bufferData, sizeInBytes);
	mCommandList->SetGraphicsRootConstantBufferView(rootParameterIndex, heapAllococation.GPUAddress);
}

void CommandList::SetGraphics32BitConstants(uint32_t rootParameterIndex, uint32_t numConstants, const void* constants)
{
	mCommandList->SetGraphicsRoot32BitConstants(rootParameterIndex, numConstants, constants, 0);
}

void CommandList::SetCompute32BitConstants(uint32_t rootParameterIndex, uint32_t numConstants, const void* constants)
{
	mCommandList->SetComputeRoot32BitConstants(rootParameterIndex, numConstants, constants, 0);
}

void CommandList::SetVertexBuffer(uint32_t slot, const VertexBuffer& vertexBuffer)
{
	TransitionBarrier(vertexBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	auto vertexBufferView = vertexBuffer.GetVertexBufferView();
	mCommandList->IASetVertexBuffers(slot, 1, &vertexBufferView);
	TrackResource(vertexBuffer);
}

void CommandList::SetDynamicVertexBuffer(uint32_t slot, size_t numVertices, size_t vertexSize, const void* vertexBufferData)
{
	size_t bufferSize = numVertices * vertexSize;

	auto heapAllocation = mUploadBuffer->Allocate(bufferSize, vertexSize);
	memcpy(heapAllocation.CPUAddress, vertexBufferData, bufferSize);

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
	vertexBufferView.BufferLocation = heapAllocation.GPUAddress;
	vertexBufferView.SizeInBytes = static_cast<UINT>(bufferSize);
	vertexBufferView.StrideInBytes = static_cast<UINT>(vertexSize);

	mCommandList->IASetVertexBuffers(slot, 1, &vertexBufferView);
}

void CommandList::SetIndexBuffer(const IndexBuffer& indexBuffer)
{
	TransitionBarrier(indexBuffer, D3D12_RESOURCE_STATE_INDEX_BUFFER);

	auto indexBufferView = indexBuffer.GetIndexBufferView();

	mCommandList->IASetIndexBuffer(&indexBufferView);

	TrackResource(indexBuffer);
}

void CommandList::SetDynamicIndexBuffer(size_t numIndicies, DXGI_FORMAT indexFormat, const void* indexBufferData)
{
	size_t indexSizeInBytes = indexFormat == DXGI_FORMAT_R16_UINT ? 2 : 4;
	size_t bufferSize = numIndicies * indexSizeInBytes;

	auto heapAllocation = mUploadBuffer->Allocate(bufferSize, indexSizeInBytes);
	memcpy(heapAllocation.CPUAddress, indexBufferData, bufferSize);

	D3D12_INDEX_BUFFER_VIEW indexBufferView = {};
	indexBufferView.BufferLocation = heapAllocation.GPUAddress;
	indexBufferView.SizeInBytes = static_cast<UINT>(bufferSize);
	indexBufferView.Format = indexFormat;

	mCommandList->IASetIndexBuffer(&indexBufferView);
}

void CommandList::SetGraphicsDynamicStructuredBuffer(uint32_t slot, size_t numElements, size_t elementSize, const void* bufferData)
{
	size_t bufferSize = numElements * elementSize;

	auto heapAllocation = mUploadBuffer->Allocate(bufferSize, elementSize);

	memcpy(heapAllocation.CPUAddress, bufferData, bufferSize);

	mCommandList->SetGraphicsRootShaderResourceView(slot, heapAllocation.GPUAddress);
}
void CommandList::SetViewport(const D3D12_VIEWPORT& viewport)
{
	SetViewports({ viewport });
}

void CommandList::SetViewports(const std::vector<D3D12_VIEWPORT>& viewports)
{
	assert(viewports.size() < D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
	mCommandList->RSSetViewports(static_cast<UINT>(viewports.size()), viewports.data());
}

void CommandList::SetScissorRect(const D3D12_RECT& scissorRect)
{
	SetScissorRects({ scissorRect });
}

void CommandList::SetScissorRects(const std::vector<D3D12_RECT>& scissorRects)
{
	assert(scissorRects.size() < D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
	mCommandList->RSSetScissorRects(static_cast<UINT>(scissorRects.size()), scissorRects.data());
}

void CommandList::SetPipelineState(Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState)
{
	mCommandList->SetPipelineState(pipelineState.Get());

	TrackResource(pipelineState);
}

void CommandList::SetGraphicsRootSignature(const RootSignature& rootSignature)
{
	auto d3d12RootSignature = rootSignature.GetRootSignature().Get();
	if (mRootSignature != d3d12RootSignature)
	{
		mRootSignature = d3d12RootSignature;

		for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
		{
			mDynamicDescriptorHeap[i]->ParseRootSignature(rootSignature);
		}

		mCommandList->SetGraphicsRootSignature(mRootSignature);

		TrackResource(mRootSignature);
	}
}

void CommandList::SetComputeRootSignature(const RootSignature& rootSignature)
{
	auto d3d12RootSignature = rootSignature.GetRootSignature().Get();
	if (mRootSignature != d3d12RootSignature)
	{
		mRootSignature = d3d12RootSignature;

		for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
		{
			mDynamicDescriptorHeap[i]->ParseRootSignature(rootSignature);
		}

		mCommandList->SetComputeRootSignature(mRootSignature);

		TrackResource(mRootSignature);
	}
}

void CommandList::SetShaderResourceView(uint32_t rootParameterIndex, uint32_t descriptorOffset, const Resource& resource, D3D12_RESOURCE_STATES stateAfter, UINT firstSubresource,
										UINT numSubresources, const D3D12_SHADER_RESOURCE_VIEW_DESC* srv)
{
	if (numSubresources < D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
	{
		for (uint32_t i = 0; i < numSubresources; ++i)
		{
			TransitionBarrier(resource, stateAfter, firstSubresource + i);
		}
	}
	else
	{
		TransitionBarrier(resource, stateAfter);
	}

	mDynamicDescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->CachingDescriptors(rootParameterIndex, descriptorOffset, 1, resource.GetShaderResourceView(srv));

	TrackResource(resource);
}

void CommandList::SetUnorderedAccessView(uint32_t rootParameterIndex, uint32_t descrptorOffset, const Resource& resource, D3D12_RESOURCE_STATES stateAfter, UINT firstSubresource,
										 UINT numSubresources, const D3D12_UNORDERED_ACCESS_VIEW_DESC* uav)
{
	if (numSubresources < D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
	{
		for (uint32_t i = 0; i < numSubresources; ++i)
		{
			TransitionBarrier(resource, stateAfter, firstSubresource + i);
		}
	}
	else
	{
		TransitionBarrier(resource, stateAfter);
	}
	mDynamicDescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->CachingDescriptors(rootParameterIndex, descrptorOffset, 1, resource.GetUnorderedAccessView(uav));
	TrackResource(resource);
}


void CommandList::SetRenderTarget(const RenderTarget& renderTarget)
{
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> renderTargetDescriptors;
	renderTargetDescriptors.reserve(AttachmentPoint::NumAttachmentPoints);

	const auto& textures = renderTarget.GetTextures();

	for (int i = 0; i < 8; ++i)
	{
		auto& texture = textures[i];

		if (texture.IsValid())
		{
			TransitionBarrier(texture, D3D12_RESOURCE_STATE_RENDER_TARGET);
			renderTargetDescriptors.push_back(texture.GetRenderTargetView());

			TrackResource(texture);
		}
	}

	const auto& depthTexture = renderTarget.GetTexture(AttachmentPoint::DepthStencil);

	CD3DX12_CPU_DESCRIPTOR_HANDLE depthStencilDescriptor(D3D12_DEFAULT);
	if (depthTexture.GetD3D12Resource())
	{
		TransitionBarrier(depthTexture, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		depthStencilDescriptor = depthTexture.GetDepthStencilView();

		TrackResource(depthTexture);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE* pDSV = depthStencilDescriptor.ptr != 0 ? &depthStencilDescriptor : nullptr;

	mCommandList->OMSetRenderTargets(static_cast<UINT>(renderTargetDescriptors.size()), renderTargetDescriptors.data(), FALSE, pDSV);
}

void CommandList::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t startVertex, uint32_t startInstance)
{
	FlushResourceBarriers();

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		mDynamicDescriptorHeap[i]->CommitCachingDescriptorsForDraw(*this);
	}

	mCommandList->DrawInstanced(vertexCount, instanceCount, startVertex, startInstance);
}

void CommandList::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t startIndex, int32_t baseVertex, uint32_t startInstance)
{
	FlushResourceBarriers();

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		mDynamicDescriptorHeap[i]->CommitCachingDescriptorsForDraw(*this);
	}

	mCommandList->DrawIndexedInstanced(indexCount, instanceCount, startIndex, baseVertex, startInstance);
}

void CommandList::Dispatch(uint32_t numGroupsX, uint32_t numGroupsY, uint32_t numGroupsZ)
{
	FlushResourceBarriers();

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		mDynamicDescriptorHeap[i]->CommitCachingDescriptorsForDispatch(*this);
	}

	mCommandList->Dispatch(numGroupsX, numGroupsY, numGroupsZ);
}

bool CommandList::Close(CommandList& pendingCommandList)
{

	FlushResourceBarriers();

	mCommandList->Close();

	uint32_t numPendingBarriers = mResourceStateTracker->FlushPendingResourceBarriers(pendingCommandList);

	mResourceStateTracker->CommitFinalResourceStates();

	return numPendingBarriers > 0;
}

void CommandList::Close()
{
	FlushResourceBarriers();
	mCommandList->Close();
}


void CommandList::Reset()
{
	ThrowIfFailed(mCommandAllocator->Reset());
	ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));

	mResourceStateTracker->Reset();
	mUploadBuffer->Reset();

	ReleaseTrackedObjects();

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		mDynamicDescriptorHeap[i]->Reset();
		mDescriptorHeaps[i] = nullptr;
	}

	mRootSignature = nullptr;
	mComputeCommandList = nullptr;
}

void CommandList::TrackResource(Microsoft::WRL::ComPtr<ID3D12Object> object)
{
	m_TrackedObjects.push_back(object);
}

void CommandList::TrackResource(const Resource& res)
{
	TrackResource(res.GetD3D12Resource());
}

void CommandList::ReleaseTrackedObjects()
{
	m_TrackedObjects.clear();
}

void CommandList::SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, ID3D12DescriptorHeap* heap)
{
	if (mDescriptorHeaps[heapType] != heap)
	{
		mDescriptorHeaps[heapType] = heap;
		BindDescriptorHeaps();
	}
}

void CommandList::BindDescriptorHeaps()
{
	UINT numDescriptorHeaps = 0;
	ID3D12DescriptorHeap* descriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] = {};

	for (uint32_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		ID3D12DescriptorHeap* descriptorHeap = mDescriptorHeaps[i];
		if (descriptorHeap)
		{
			descriptorHeaps[numDescriptorHeaps++] = descriptorHeap;
		}
	}

	mCommandList->SetDescriptorHeaps(numDescriptorHeaps, descriptorHeaps);
}