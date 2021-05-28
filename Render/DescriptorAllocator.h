#ifndef __DESCRIPTORALLOCATOR_H_
#define __DESCRIPTORALLOCATOR_H_

#include "Core.h"
#include "DescriptorAllocationBlock.h"

class DescriptorBlock;

class DescriptorAllocator
{
public:
	DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptorsPerHeap = 256);
	virtual ~DescriptorAllocator();

	DescriptorAllocationBlock Allocate(uint32_t descriptorNum = 1);
	void ReleaseUsedDescriptors(uint64_t frameNumber);

private:
	using DescriptorHeapPool = std::vector<std::shared_ptr<DescriptorBlock>>;
	std::shared_ptr<DescriptorBlock> CreateDescriptorBlock();
	D3D12_DESCRIPTOR_HEAP_TYPE mHeapType;
	uint32_t mDescriptorNumPerHeap;
	DescriptorHeapPool mHeapPool;
	std::set<size_t> mFreeHeaps;
	std::mutex mAllocationBlockMutex;
};

#endif
