#include "UploadBuffer.h"
#include "Application.h"
#include "Helpers.h"

UploadBuffer::UploadBuffer(size_t blockSize) : mBlockSize(blockSize) {}

UploadBuffer::~UploadBuffer() {}

UploadBuffer::BasePointer UploadBuffer::Allocate(size_t allocateSize, size_t alignment)
{
	if (allocateSize > mBlockSize)
	{
		throw std::bad_alloc();
	}

	if (!mUsingBlock || !mUsingBlock->HasSpace(allocateSize, alignment))
	{
		mUsingBlock = MakeBlock();
	}
	return mUsingBlock->Allocate(allocateSize, alignment);
}

std::shared_ptr<UploadBuffer::Block> UploadBuffer::MakeBlock()
{
	std::shared_ptr<Block> block;
	if (!mFreeBlockPool.empty())
	{
		block = mFreeBlockPool.front();
		mFreeBlockPool.pop_front();
	}
	else
	{
		block = std::make_shared<Block>(mBlockSize);
		mBlockPool.push_back(block);
	}
	return block;
}

void UploadBuffer::Reset()
{
	mUsingBlock = nullptr;
	mFreeBlockPool = mBlockPool;
	for (auto block : mFreeBlockPool)
	{
		block->Reset();
	}
}

UploadBuffer::Block::Block(size_t size) : mBlockSize(size), mOffset(0), mCPUBasePointer(nullptr), mGPUBasePointer(D3D12_GPU_VIRTUAL_ADDRESS(0))
{
	auto device = Application::Get().GetDevice();
	ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(mBlockSize),
												  D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mResource)));
	mGPUBasePointer = mResource->GetGPUVirtualAddress();
	mResource->Map(0, nullptr, &mCPUBasePointer);
}

UploadBuffer::Block::~Block()
{
	mResource->Unmap(0, nullptr);
	mCPUBasePointer = nullptr;
	mGPUBasePointer = D3D12_GPU_VIRTUAL_ADDRESS(0);
}

bool UploadBuffer::Block::HasSpace(size_t blockSize, size_t alignment) const
{
	size_t alignedSize = Math::AlignUp(blockSize, alignment);
	size_t alignedOffset = Math::AlignUp(mOffset, alignment);
	return alignedOffset + alignedSize <= mBlockSize;
}



UploadBuffer::BasePointer UploadBuffer::Block::Allocate(size_t allocateSize, size_t alignment)
{
	if (!HasSpace(allocateSize, alignment))
	{
		throw std::bad_alloc();
	}

	size_t alignedSize = Math::AlignUp(allocateSize, alignment);
	mOffset = Math::AlignUp(mOffset, alignment);
	BasePointer allocateBlockPointer;
	allocateBlockPointer.CPUAddress = static_cast<uint8_t*>(mCPUBasePointer) + mOffset;
	allocateBlockPointer.GPUAddress = mGPUBasePointer + mOffset;
	mOffset += alignedSize;
	return allocateBlockPointer;
}
void UploadBuffer::Block::Reset()
{
	mOffset = 0;
}