#include "CommandQueue.h"
#include "Application.h"
#include "CommandList.h"
#include "ResourceStateTracker.h"


CommandQueue::CommandQueue(D3D12_COMMAND_LIST_TYPE type) : mFenceValue(0) , mCommandListType(type) , mbProcessInFlightCommandLists(true)
{
	auto device = Application::Get().GetDevice();

	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = type;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask = 0;

	ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&mCommandQueue)));
	ThrowIfFailed(device->CreateFence(mFenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));

	switch (type)
	{
	case D3D12_COMMAND_LIST_TYPE_COPY:
		mCommandQueue->SetName(L"Copy Command Queue");
		break;
	case D3D12_COMMAND_LIST_TYPE_COMPUTE:
		mCommandQueue->SetName(L"Compute Command Queue");
		break;
	case D3D12_COMMAND_LIST_TYPE_DIRECT:
		mCommandQueue->SetName(L"Direct Command Queue");
		break;
	}

	mProcessInFlightCommandListsThread = std::thread(&CommandQueue::ProccessInFlightCommandLists, this);
}

CommandQueue::~CommandQueue()
{
	mbProcessInFlightCommandLists = false;
	mProcessInFlightCommandListsThread.join();
}

uint64_t CommandQueue::Signal()
{
	uint64_t fenceValue = ++mFenceValue;
	mCommandQueue->Signal(mFence.Get(), fenceValue);
	return fenceValue;
}

bool CommandQueue::IsFenceComplete(uint64_t fenceValue)
{
	return mFence->GetCompletedValue() >= fenceValue;
}

void CommandQueue::WaitForFenceValue(uint64_t fenceValue)
{
	if (!IsFenceComplete(fenceValue))
	{
		auto event = ::CreateEvent(NULL, FALSE, FALSE, NULL);
		assert(event && "创建fence事件句柄失败");
		mFence->SetEventOnCompletion(fenceValue, event);
		WaitForSingleObject(event, DWORD_MAX);

		CloseHandle(event);
	}
}

void CommandQueue::Flush()
{
	std::unique_lock<std::mutex> lock(mProcessInFlightCommandListsThreadMutex);
	mProcessInFlightCommandListsThreadCV.wait(lock, [this] { return mInFlightCommandLists.Empty(); });
	WaitForFenceValue(mFenceValue);
}

std::shared_ptr<CommandList> CommandQueue::GetCommandList()
{
	std::shared_ptr<CommandList> commandList;

	if (!mAvailableCommandLists.Empty())
	{
		mAvailableCommandLists.TryPop(commandList);
	}
	else
	{
		commandList = std::make_shared<CommandList>(mCommandListType);
	}

	return commandList;
}

uint64_t CommandQueue::ExecuteCommandList(std::shared_ptr<CommandList> commandList)
{
	return ExecuteCommandLists(std::vector<std::shared_ptr<CommandList>>({ commandList }));
}

uint64_t CommandQueue::ExecuteCommandLists(const std::vector<std::shared_ptr<CommandList>>& commandLists)
{
	ResourceStateTracker::Lock();

	std::vector<std::shared_ptr<CommandList>> toBeQueued;
	toBeQueued.reserve(commandLists.size() * 2);       

	std::vector<std::shared_ptr<CommandList>> generateMipsCommandLists;
	generateMipsCommandLists.reserve(commandLists.size());

	std::vector<ID3D12CommandList*> d3d12CommandLists;
	d3d12CommandLists.reserve(commandLists.size() * 2); 
	for (auto commandList : commandLists)
	{
		auto pendingCommandList = GetCommandList();
		bool hasPendingBarriers = commandList->Close(*pendingCommandList);
		pendingCommandList->Close();
		if (hasPendingBarriers)
		{
			d3d12CommandLists.push_back(pendingCommandList->GetGraphicsCommandList().Get());
		}
		d3d12CommandLists.push_back(commandList->GetGraphicsCommandList().Get());

		toBeQueued.push_back(pendingCommandList);
		toBeQueued.push_back(commandList);

		auto generateMipsCommandList = commandList->GetGenerateMipsCommandList();
		if (generateMipsCommandList)
		{
			generateMipsCommandLists.push_back(generateMipsCommandList);
		}
	}

	UINT numCommandLists = static_cast<UINT>(d3d12CommandLists.size());
	mCommandQueue->ExecuteCommandLists(numCommandLists, d3d12CommandLists.data());
	uint64_t fenceValue = Signal();

	ResourceStateTracker::Unlock();

	for (auto commandList : toBeQueued)
	{
		mInFlightCommandLists.Push({ fenceValue, commandList });
	}

	if (generateMipsCommandLists.size() > 0)
	{
		auto computeQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE);
		computeQueue->Wait(*this);
		computeQueue->ExecuteCommandLists(generateMipsCommandLists);
	}

	return fenceValue;
}

void CommandQueue::Wait(const CommandQueue& other)
{
	mCommandQueue->Wait(other.mFence.Get(), other.mFenceValue);
}

Microsoft::WRL::ComPtr<ID3D12CommandQueue> CommandQueue::GetD3D12CommandQueue() const
{
	return mCommandQueue;
}

void CommandQueue::ProccessInFlightCommandLists()
{
	std::unique_lock<std::mutex> lock(mProcessInFlightCommandListsThreadMutex, std::defer_lock);

	while (mbProcessInFlightCommandLists)
	{
		CommandListEntry commandListEntry;

		lock.lock();
		while (mInFlightCommandLists.TryPop(commandListEntry))
		{
			auto fenceValue = std::get<0>(commandListEntry);
			auto commandList = std::get<1>(commandListEntry);

			WaitForFenceValue(fenceValue);

			commandList->Reset();

			mAvailableCommandLists.Push(commandList);
		}
		lock.unlock();
		mProcessInFlightCommandListsThreadCV.notify_one();
		std::this_thread::yield();
	}
}