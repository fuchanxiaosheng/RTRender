#include "DescriptorAllocationBlock.h"

#include "Application.h"
#include "DescriptorBlock.h"

DescriptorAllocationBlock::DescriptorAllocationBlock() : mDescriptor{0}, mHandleNum(0), mDescriptorSize(0), mBlock(nullptr) {}

DescriptorAllocationBlock::DescriptorAllocationBlock(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, uint32_t handleNum, uint32_t descriptorSize, std::shared_ptr<DescriptorBlock> block)
	:mDescriptor(descriptor), mHandleNum(handleNum), mDescriptorSize(descriptorSize), mBlock(block) {}

DescriptorAllocationBlock::~DescriptorAllocationBlock()
{
	Free();
}

DescriptorAllocationBlock::DescriptorAllocationBlock(DescriptorAllocationBlock&& allocationBlock)
	:mDescriptor(allocationBlock.mDescriptor), mHandleNum(allocationBlock.mHandleNum), mDescriptorSize(allocationBlock.mDescriptorSize), mBlock(std::move(allocationBlock.mBlock))
{
	allocationBlock.mDescriptor.ptr = 0;
	allocationBlock.mHandleNum = 0;
	allocationBlock.mDescriptorSize = 0;
}
DescriptorAllocationBlock& DescriptorAllocationBlock::operator=(DescriptorAllocationBlock&& otherBlock)
{
	Free();
	mDescriptor = otherBlock.mDescriptor;
	mHandleNum = otherBlock.mHandleNum;
	mDescriptorSize = otherBlock.mDescriptorSize;
	mBlock = std::move(otherBlock.mBlock);

	otherBlock.mDescriptor.ptr = 0;
	otherBlock.mHandleNum = 0;
	otherBlock.mDescriptorSize = 0;
	return *this;
}

bool DescriptorAllocationBlock::IsNull() const
{
	return mDescriptor.ptr == 0;
}
D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocationBlock::GetDescriptorHandle(uint32_t offset) const
{
	assert(offset < mHandleNum);
	return { mDescriptor.ptr + (mDescriptorSize * offset) };
}
uint32_t DescriptorAllocationBlock::GetHandleNum() const
{
	return mHandleNum;
}
std::shared_ptr<DescriptorBlock> DescriptorAllocationBlock::GetDescriptorBlock() const
{
	return mBlock;
}

void DescriptorAllocationBlock::Free()
{
	if (!IsNull() && mBlock)
	{
		mBlock->Free(std::move(*this), Application::GetFrameCount());
		mDescriptor.ptr = 0;
		mHandleNum = 0;
		mDescriptorSize = 0;
		mBlock.reset();
	}
}