#ifndef __DESCRIPTORBLOCK_H_
#define __DESCRIPTORBLOCK_H_
#include "Core.h"
#include "DescriptorAllocationBlock.h"

class DescriptorBlock : public std::enable_shared_from_this<DescriptorBlock>
{
public:
	DescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t descriptorNum);
	D3D12_DESCRIPTOR_HEAP_TYPE GetHeapType() const;
	bool HasSpace(uint32_t descriptorNum) const;
	uint32_t GetFreeHandleNum() const;
	DescriptorAllocationBlock Allocate(uint32_t descriptorNum);
	void Free(DescriptorAllocationBlock&& blockHandle, uint64_t frameNumber);
	void ReleaseDescriptors(uint64_t frameNumber);
protected:
	uint32_t ComputeOffset(D3D12_CPU_DESCRIPTOR_HANDLE handle);
	void AddNewBlock(uint32_t offset, uint32_t descriptorNum);
	void FreeBlock(uint32_t offset, uint32_t descriptorNum);

private:

	struct FreeBlockInfo
	{
		FreeBlockInfo(uint32_t size) : Size(size) {}
		uint32_t Size;
		typename std::multimap<uint32_t, typename std::map<uint32_t, FreeBlockInfo>::iterator>::iterator FreeSizeListIterator;
	};

	struct UsedDescriptorInfo
	{
		UsedDescriptorInfo(uint32_t offset, uint32_t size, uint64_t frame) : Offset(offset), Size(size), FrameNumber(frame) {}

		uint32_t Offset;
		uint32_t Size;
		uint64_t FrameNumber;
	};

	using UsedDescriptorQueue = std::queue<UsedDescriptorInfo>;
	using FreeOffsetList = std::map<uint32_t, FreeBlockInfo>;
	using FreeSizeList = std::multimap<uint32_t, FreeOffsetList::iterator>;
	FreeOffsetList mFreeOffsetList;
	FreeSizeList mFreeSizeList;
	UsedDescriptorQueue mUsedDescriptors;

	ComPtr<ID3D12DescriptorHeap> mDescriptorHeap;
	D3D12_DESCRIPTOR_HEAP_TYPE mHeapType;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mBaseDescriptor;
	uint32_t mDescritptorHandleIncrementSize;
	uint32_t mDescriptorNumInHeap;
	uint32_t mFreeHandleNum;
	std::mutex mDescriptorAllocationBlockMutex;
};

#endif
