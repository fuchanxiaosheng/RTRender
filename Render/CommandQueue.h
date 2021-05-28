#ifndef __COMMANDQUEUE_H_
#define __COMMANDQUEUE_H_

 
#include "Core.h"
#include "ThreadSafeQueue.h"
class CommandList;
class CommandQueue
{
public:
	CommandQueue(D3D12_COMMAND_LIST_TYPE type);
	virtual ~CommandQueue();
	std::shared_ptr<CommandList> GetCommandList();
	uint64_t ExecuteCommandList(std::shared_ptr<CommandList> commandList);
	uint64_t ExecuteCommandLists(const std::vector<std::shared_ptr<CommandList>>& commandLists);

	uint64_t Signal();
	bool IsFenceComplete(uint64_t fenceValue);
	void WaitForFenceValue(uint64_t fenceValue);
	void Flush();

	void Wait(const CommandQueue& other);
	ComPtr<ID3D12CommandQueue> GetD3D12CommandQueue() const;

private:
	void ProccessInFlightCommandLists();
	using CommandListEntry = std::tuple<uint64_t, std::shared_ptr<CommandList> >;

	D3D12_COMMAND_LIST_TYPE mCommandListType;
	ComPtr<ID3D12CommandQueue> mCommandQueue;
	ComPtr<ID3D12Fence> mFence;
	std::atomic_uint64_t  mFenceValue;

	ThreadSafeQueue<CommandListEntry>  mInFlightCommandLists;
	ThreadSafeQueue<std::shared_ptr<CommandList>> mAvailableCommandLists;

	std::thread mProcessInFlightCommandListsThread;
	std::atomic_bool mbProcessInFlightCommandLists;
	std::mutex mProcessInFlightCommandListsThreadMutex;
	std::condition_variable mProcessInFlightCommandListsThreadCV;
};

#endif