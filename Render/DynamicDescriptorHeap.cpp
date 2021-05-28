#include "DynamicDescriptorHeap.h"

#include "Application.h"
#include "CommandList.h"
#include "RootSignature.h"

DynamicDescriptorHeap::DynamicDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t dercriptorNumPerHeap)
	:mDescriptorHeapType(heapType), mDescriptorNumPerHeap(dercriptorNumPerHeap), mDescriptorTableBitMask(0), mUsedDescriptorTableBiteMask(0)
	,mCurrentCPUDescriptorHandle(D3D12_DEFAULT), mCurrentGPUDescriptorHandle(D3D12_DEFAULT), mFreeHandleNum(0)
{
	mDescriptorHandleIncrementSize = Application::Get().GetDescriptorHandleIncrementSize(heapType);
	mDescriptorHandleCache = std::make_unique<D3D12_CPU_DESCRIPTOR_HANDLE[]>(mDescriptorNumPerHeap);
}
DynamicDescriptorHeap::~DynamicDescriptorHeap(){}

void DynamicDescriptorHeap::CachingDescriptors(uint32_t rootParameterIndex, uint32_t offset, uint32_t descriptorNum, const D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptors)
{
	if (descriptorNum > mDescriptorNumPerHeap || rootParameterIndex >= MaxDescriptorTables)
	{
		throw std::bad_alloc();
	}
	DescriptorTableCache& descriptorTableCache = mDescriptorTableCache[rootParameterIndex];
	if ((offset + descriptorNum) > descriptorTableCache.DescriptorNum)
	{
		throw std::length_error("描述符的数量超过描述符表内的描述符的个数");
	}
	D3D12_CPU_DESCRIPTOR_HANDLE* dstDescriptor = (descriptorTableCache.BaseDescriptor + offset);
	for (uint32_t i = 0; i < descriptorNum; i++)
	{
		dstDescriptor[i] = CD3DX12_CPU_DESCRIPTOR_HANDLE(srcDescriptors, i, mDescriptorHandleIncrementSize);
	}
	mUsedDescriptorTableBiteMask |= (1 << rootParameterIndex);
}

void DynamicDescriptorHeap::CommitCachingDescriptors(CommandList& commandList, std::function<void(ID3D12GraphicsCommandList*, UINT, D3D12_GPU_DESCRIPTOR_HANDLE)> setFunc)
{
	uint32_t commitingDescriptorNum = ComputeUsedDescriptorCount();
	if (commitingDescriptorNum > 0)
	{
		auto device = Application::Get().GetDevice();
		auto graphicsCommandList = commandList.GetGraphicsCommandList().Get();
		assert(graphicsCommandList != nullptr);

		if (!mCurrentDescriptorHeap || mFreeHandleNum < commitingDescriptorNum)
		{
			mCurrentDescriptorHeap = RequestDescriptorHeap();
			mCurrentCPUDescriptorHandle = mCurrentDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			mCurrentGPUDescriptorHandle = mCurrentDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
			mFreeHandleNum = mDescriptorNumPerHeap;
			commandList.SetDescriptorHeap(mDescriptorHeapType, mCurrentDescriptorHeap.Get());
			mUsedDescriptorTableBiteMask = mDescriptorTableBitMask;
		}
		DWORD rootIndex;
		while (_BitScanForward(&rootIndex, mUsedDescriptorTableBiteMask))
		{
			UINT srcDescriptorNum = mDescriptorTableCache[rootIndex].DescriptorNum;
			D3D12_CPU_DESCRIPTOR_HANDLE* pSrcDescriptorHandles = mDescriptorTableCache[rootIndex].BaseDescriptor;
			D3D12_CPU_DESCRIPTOR_HANDLE pDestDescriptorRangeStarts[] = { mCurrentCPUDescriptorHandle };
			UINT pDestDescriptorRangeSizes[] = { srcDescriptorNum };
			device->CopyDescriptors(1, pDestDescriptorRangeStarts, pDestDescriptorRangeSizes, srcDescriptorNum, pSrcDescriptorHandles, nullptr, mDescriptorHeapType);
			setFunc(graphicsCommandList, rootIndex, mCurrentGPUDescriptorHandle);
			mCurrentCPUDescriptorHandle.Offset(srcDescriptorNum, mDescriptorHandleIncrementSize);
			mCurrentGPUDescriptorHandle.Offset(srcDescriptorNum, mDescriptorHandleIncrementSize);
			mFreeHandleNum -= srcDescriptorNum;
			mUsedDescriptorTableBiteMask ^= (1 << rootIndex);
		}
	}
}
void DynamicDescriptorHeap::CommitCachingDescriptorsForDraw(CommandList& commandList)
{
	CommitCachingDescriptors(commandList, &ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable);
}
void DynamicDescriptorHeap::CommitCachingDescriptorsForDispatch(CommandList& commandList)
{
	CommitCachingDescriptors(commandList, &ID3D12GraphicsCommandList::SetComputeRootDescriptorTable);
}

D3D12_GPU_DESCRIPTOR_HANDLE DynamicDescriptorHeap::CopyDescriptor(CommandList& commandList, D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor)
{
	if (!mCurrentDescriptorHeap || mFreeHandleNum < 1)
	{
		mCurrentDescriptorHeap = RequestDescriptorHeap();
		mCurrentCPUDescriptorHandle = mCurrentDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		mCurrentGPUDescriptorHandle = mCurrentDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		mFreeHandleNum = mDescriptorNumPerHeap;
		commandList.SetDescriptorHeap(mDescriptorHeapType, mCurrentDescriptorHeap.Get());
		mUsedDescriptorTableBiteMask = mDescriptorTableBitMask;
	}
	auto device = Application::Get().GetDevice();
	D3D12_GPU_DESCRIPTOR_HANDLE hGPU = mCurrentGPUDescriptorHandle;
	device->CopyDescriptorsSimple(1, mCurrentCPUDescriptorHandle, cpuDescriptor, mDescriptorHeapType);
	mCurrentCPUDescriptorHandle.Offset(1, mDescriptorHandleIncrementSize);
	mCurrentGPUDescriptorHandle.Offset(1, mDescriptorHandleIncrementSize);

	mFreeHandleNum -= 1;
	return hGPU;
}

void DynamicDescriptorHeap::ParseRootSignature(const RootSignature& rootSignature)
{
	mUsedDescriptorTableBiteMask = 0;
	const  auto& rootSignatureDesc = rootSignature.GetRootSignatureDesc();
	mDescriptorTableBitMask = rootSignature.GetDescriptorTableBitMask(mDescriptorHeapType);
	uint32_t descriptorTableBiteMask = mDescriptorTableBitMask;
	uint32_t currentOffset = 0;
	DWORD rootIndex;

	while (_BitScanForward(&rootIndex, descriptorTableBiteMask) && rootIndex < rootSignatureDesc.NumParameters)
	{
		uint32_t descriptorNum = rootSignature.GetNumDescriptors(rootIndex);
		DescriptorTableCache& descriptorTableCache = mDescriptorTableCache[rootIndex];
		descriptorTableCache.DescriptorNum = descriptorNum;
		descriptorTableCache.BaseDescriptor = mDescriptorHandleCache.get() + currentOffset;
		currentOffset += descriptorNum;
		descriptorTableBiteMask ^= (1 << rootIndex);
	}
	assert(currentOffset <= mDescriptorNumPerHeap && "root signature要求超过描述符堆的描述符个数");
}
void DynamicDescriptorHeap::Reset()
{
	mFreeDescriptorPool = mDescriptorHeapPool;
	mCurrentDescriptorHeap.Reset();
	mCurrentCPUDescriptorHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
	mCurrentGPUDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
	mFreeHandleNum = 0;
	mDescriptorTableBitMask = 0;
	mUsedDescriptorTableBiteMask = 0;
	for (int i = 0; i < MaxDescriptorTables; i++)
	{
		mDescriptorTableCache[i].Reset();
	}
}


ComPtr<ID3D12DescriptorHeap> DynamicDescriptorHeap::RequestDescriptorHeap()
{
	ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	if (!mFreeDescriptorPool.empty())
	{
		descriptorHeap = mFreeDescriptorPool.front();
		mFreeDescriptorPool.pop();
	}
	else
	{
		descriptorHeap = CreateDescriptorHeap();
		mDescriptorHeapPool.push(descriptorHeap);
	}
	return descriptorHeap;
}
ComPtr<ID3D12DescriptorHeap> DynamicDescriptorHeap::CreateDescriptorHeap()
{
	auto device = Application::Get().GetDevice();
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
	descriptorHeapDesc.Type = mDescriptorHeapType;
	descriptorHeapDesc.NumDescriptors = mDescriptorNumPerHeap;
	descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	ThrowIfFailed(device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap)));
	return descriptorHeap;
}
uint32_t DynamicDescriptorHeap::ComputeUsedDescriptorCount() const
{
	uint32_t usedDescriptorNum = 0;
	DWORD i;
	DWORD usedDescriptorBiteMask = mUsedDescriptorTableBiteMask;

	while(_BitScanForward(&i, usedDescriptorBiteMask))
	{
		usedDescriptorNum += mDescriptorTableCache[i].DescriptorNum;
		usedDescriptorBiteMask ^= (1 << i);
	}
	return usedDescriptorNum;
}