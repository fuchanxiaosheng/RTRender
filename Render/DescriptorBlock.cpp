#include "DescriptorBlock.h"
#include "Application.h"

DescriptorBlock::DescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t descriptorNum) : mHeapType(type), mDescriptorNumInHeap(descriptorNum)
{
	auto device = Application::Get().GetDevice();

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = mHeapType;
	heapDesc.NumDescriptors = mDescriptorNumInHeap;
	ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mDescriptorHeap)));
	mBaseDescriptor = mDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	mDescritptorHandleIncrementSize = device->GetDescriptorHandleIncrementSize(mHeapType);
	mFreeHandleNum = mDescriptorNumInHeap;

	AddNewBlock(0, mFreeHandleNum);
}
D3D12_DESCRIPTOR_HEAP_TYPE DescriptorBlock::GetHeapType() const
{
	return mHeapType;
}
bool DescriptorBlock::HasSpace(uint32_t descriptorNum) const
{
	return mFreeSizeList.lower_bound(descriptorNum) != mFreeSizeList.end();
}
uint32_t DescriptorBlock::GetFreeHandleNum() const
{
	return mFreeHandleNum;
}
DescriptorAllocationBlock DescriptorBlock::Allocate(uint32_t descriptorNum)
{
	std::lock_guard<std::mutex> lock(mDescriptorAllocationBlockMutex);
	if (descriptorNum > mFreeHandleNum)
	{
		return DescriptorAllocationBlock();
	}
	auto smallestBlockIterator = mFreeSizeList.lower_bound(descriptorNum);
	if (smallestBlockIterator == mFreeSizeList.end())
	{
		return DescriptorAllocationBlock();
	}
	auto blockSize = smallestBlockIterator->first;
	auto offsetIterator = smallestBlockIterator->second;
	auto offset = offsetIterator->first;
	mFreeSizeList.erase(smallestBlockIterator);
	mFreeOffsetList.erase(offsetIterator);
	auto newOffset = offset + descriptorNum;
	auto newSize = blockSize - descriptorNum;
	if (newSize > 0)
	{
		AddNewBlock(newOffset, newSize);
	}
	mFreeHandleNum -= descriptorNum;
	return DescriptorAllocationBlock(CD3DX12_CPU_DESCRIPTOR_HANDLE(mBaseDescriptor, offset, mDescritptorHandleIncrementSize), descriptorNum, mDescritptorHandleIncrementSize, shared_from_this());

}
void DescriptorBlock::Free(DescriptorAllocationBlock&& blockHandle, uint64_t frameNumber)
{
	auto offset = ComputeOffset(blockHandle.GetDescriptorHandle());
	std::lock_guard<std::mutex> lock(mDescriptorAllocationBlockMutex);
	mUsedDescriptors.emplace(offset, blockHandle.GetHandleNum(), frameNumber);
}
void DescriptorBlock::ReleaseDescriptors(uint64_t frameNumber)
{
	std::lock_guard<std::mutex> lock(mDescriptorAllocationBlockMutex);
	while (!mUsedDescriptors.empty() && mUsedDescriptors.front().FrameNumber <= frameNumber)
	{
		auto& usedDescriptor = mUsedDescriptors.front();
		auto offset = usedDescriptor.Offset;
		auto descriptorNum = usedDescriptor.Size;
		FreeBlock(offset, descriptorNum);
		mUsedDescriptors.pop();
	}
}

uint32_t DescriptorBlock::ComputeOffset(D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
	return static_cast<uint32_t>(handle.ptr - mBaseDescriptor.ptr) / mDescritptorHandleIncrementSize;
}
void DescriptorBlock::AddNewBlock(uint32_t offset, uint32_t descriptorNum)
{
	auto offsetIterator = mFreeOffsetList.emplace(offset, descriptorNum);
	auto sizeIterator = mFreeSizeList.emplace(descriptorNum, offsetIterator.first);
	offsetIterator.first->second.FreeSizeListIterator = sizeIterator;
}
void DescriptorBlock::FreeBlock(uint32_t offset, uint32_t descriptorNum)
{
	auto nextBlockIterator = mFreeOffsetList.upper_bound(offset);
	auto prevBlockIterator = nextBlockIterator;
	if (prevBlockIterator != mFreeOffsetList.begin())
	{
		prevBlockIterator--;
	}
	else
	{
		prevBlockIterator = mFreeOffsetList.end();
	}
	mFreeHandleNum += descriptorNum;
	if (prevBlockIterator != mFreeOffsetList.end() && offset == prevBlockIterator->first + prevBlockIterator->second.Size)
	{
		offset = prevBlockIterator->first;
		descriptorNum += prevBlockIterator->second.Size;
		mFreeSizeList.erase(prevBlockIterator->second.FreeSizeListIterator);
		mFreeOffsetList.erase(prevBlockIterator);
	}
	if (nextBlockIterator != mFreeOffsetList.end() && offset + descriptorNum == nextBlockIterator->first)
	{
		descriptorNum += nextBlockIterator->second.Size;
		mFreeSizeList.erase(nextBlockIterator->second.FreeSizeListIterator);
		mFreeOffsetList.erase(nextBlockIterator);
	}
	AddNewBlock(offset, descriptorNum);
}