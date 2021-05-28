#ifndef __DESCRIPTORALLOCATIONBLOCK_H_
#define __DESCRIPTORALLOCATIONBLOCK_H_

#include "Core.h"


class DescriptorBlock;
class DescriptorAllocationBlock
{	
public:
	DescriptorAllocationBlock();

	DescriptorAllocationBlock(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, uint32_t handleNum, uint32_t descriptorSize, std::shared_ptr<DescriptorBlock> block);

	~DescriptorAllocationBlock();

	DescriptorAllocationBlock(const DescriptorAllocationBlock&) = delete;
	DescriptorAllocationBlock& operator=(const DescriptorAllocationBlock&) = delete;

	DescriptorAllocationBlock(DescriptorAllocationBlock&& allocationBlock);
	DescriptorAllocationBlock& operator=(DescriptorAllocationBlock&& otherBlock);

	bool IsNull() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHandle(uint32_t offset = 0) const;
	uint32_t GetHandleNum() const;
	std::shared_ptr<DescriptorBlock> GetDescriptorBlock() const;

private:
	void Free();
	D3D12_CPU_DESCRIPTOR_HANDLE mDescriptor;
	uint32_t mHandleNum;
	uint32_t mDescriptorSize;
	std::shared_ptr<DescriptorBlock> mBlock;
};

#endif
