#ifndef __DYNAMICDESCRIPTORHEAP_H_
#define __DYNAMICDESCRIPTORHEAP_H_

#include "Core.h"


class CommandList;
class RootSignature;

class DynamicDescriptorHeap
{
public:
	DynamicDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t dercriptorNumPerHeap = 1024);
	virtual ~DynamicDescriptorHeap();
	void CachingDescriptors(uint32_t rootParameterIndex, uint32_t offset, uint32_t descriptorNum, const D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptors);

	void CommitCachingDescriptors(CommandList& commandList, std::function<void(ID3D12GraphicsCommandList*, UINT, D3D12_GPU_DESCRIPTOR_HANDLE)> setFunc);
	void CommitCachingDescriptorsForDraw(CommandList& commandList);
	void CommitCachingDescriptorsForDispatch(CommandList& commandList);

	D3D12_GPU_DESCRIPTOR_HANDLE CopyDescriptor(CommandList& commandList, D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor);

	void ParseRootSignature(const RootSignature& rootSignature);
	void Reset();

private:
	ComPtr<ID3D12DescriptorHeap> RequestDescriptorHeap();
	ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap();
	uint32_t ComputeUsedDescriptorCount() const;
	static const uint32_t MaxDescriptorTables = 32;

	struct DescriptorTableCache
	{
		DescriptorTableCache() : DescriptorNum(0), BaseDescriptor(nullptr) {}
		void Reset()
		{
			DescriptorNum = 0;
			BaseDescriptor = nullptr;
		}
		uint32_t DescriptorNum;
		D3D12_CPU_DESCRIPTOR_HANDLE* BaseDescriptor;
	};

	D3D12_DESCRIPTOR_HEAP_TYPE mDescriptorHeapType;
	uint32_t mDescriptorNumPerHeap;
	uint32_t mDescriptorHandleIncrementSize;
	std::unique_ptr<D3D12_CPU_DESCRIPTOR_HANDLE[]> mDescriptorHandleCache;
	DescriptorTableCache mDescriptorTableCache[MaxDescriptorTables];

	uint32_t mDescriptorTableBitMask;
	uint32_t mUsedDescriptorTableBiteMask;
	using DescriptorHeapPool = std::queue<ComPtr<ID3D12DescriptorHeap>>;
	DescriptorHeapPool mDescriptorHeapPool;
	DescriptorHeapPool mFreeDescriptorPool;
	ComPtr<ID3D12DescriptorHeap> mCurrentDescriptorHeap;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mCurrentGPUDescriptorHandle;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mCurrentCPUDescriptorHandle;

	uint32_t mFreeHandleNum;
};

#endif
