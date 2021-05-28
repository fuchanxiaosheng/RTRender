#include "DescriptorAllocator.h"
#include "DescriptorBlock.h"

DescriptorAllocator::DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptorsPerHeap) : mHeapType(type), mDescriptorNumPerHeap(numDescriptorsPerHeap) {}

DescriptorAllocator::~DescriptorAllocator() {}

DescriptorAllocationBlock DescriptorAllocator::Allocate(uint32_t descriptorNum)
{
	std::lock_guard<std::mutex> lock(mAllocationBlockMutex);
	DescriptorAllocationBlock allocationBlock;
	for (auto iter = mFreeHeaps.begin(); iter != mFreeHeaps.end(); iter++)
	{
		auto allocatorBlock = mHeapPool[*iter];
		allocationBlock = allocatorBlock->Allocate(descriptorNum);
		if (allocatorBlock->GetFreeHandleNum() == 0)
		{
			iter = mFreeHeaps.erase(iter);
		}
		if (!allocationBlock.IsNull())
		{
			break;
		}
	}
	if (allocationBlock.IsNull())
	{
		mDescriptorNumPerHeap = (std::max)(mDescriptorNumPerHeap, descriptorNum);
		auto newBlock = CreateDescriptorBlock();
		allocationBlock = newBlock->Allocate(descriptorNum);
	}
	return allocationBlock;
}
void DescriptorAllocator::ReleaseUsedDescriptors(uint64_t frameNumber)
{
	std::lock_guard<std::mutex> lock(mAllocationBlockMutex);
	for (size_t i = 0; i < mHeapPool.size(); i++)
	{
		auto block = mHeapPool[i];
		block->ReleaseDescriptors(frameNumber);
		if (block->GetFreeHandleNum() > 0)
		{
			mFreeHeaps.insert(i);
		}
	}
}

std::shared_ptr<DescriptorBlock> DescriptorAllocator::CreateDescriptorBlock()
{
	auto newBlock = std::make_shared<DescriptorBlock>(mHeapType, mDescriptorNumPerHeap);
	mHeapPool.emplace_back(newBlock);
	mFreeHeaps.insert(mHeapPool.size() - 1);
	return newBlock;
}