#ifndef __UPLOADBUFFER_H_
#define __UPLOADBUFFER_H_

#include "Core.h"

#define _2MB 2*1024*1024

class UploadBuffer
{
public:
	struct BasePointer
	{
		void* CPUAddress;
		D3D12_GPU_VIRTUAL_ADDRESS GPUAddress;
	};

	explicit UploadBuffer(size_t blockSize = _2MB);
	~UploadBuffer();

	size_t GetBlockSize() const { return mBlockSize; }
	BasePointer Allocate(size_t allocateSize, size_t alignment);
	void Reset();

private:
	struct Block
	{
		Block(size_t size);
		~Block();
		bool HasSpace(size_t blockSize, size_t alignment) const;
		BasePointer Allocate(size_t allocateSize, size_t alignment);
		void Reset();

	private:
		ComPtr<ID3D12Resource> mResource;
		void* mCPUBasePointer;
		D3D12_GPU_VIRTUAL_ADDRESS mGPUBasePointer;

		size_t mBlockSize;
		size_t mOffset;

	};

	using BlockPool = std::deque<std::shared_ptr<Block>>;
	std::shared_ptr<Block> MakeBlock();
	BlockPool mBlockPool;
	BlockPool mFreeBlockPool;
	std::shared_ptr<Block> mUsingBlock;

	size_t mBlockSize;
};

#endif
