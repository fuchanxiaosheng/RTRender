#include "ResourceStateTracker.h"
#include "CommandList.h"
#include "Resource.h"

std::mutex ResourceStateTracker::msGlobalMutex;
bool ResourceStateTracker::msIsLocked = false;
ResourceStateTracker::ResourceStateMap ResourceStateTracker::msGlobalResourceState;


ResourceStateTracker::ResourceStateTracker() {}

ResourceStateTracker::~ResourceStateTracker() {}

void ResourceStateTracker::ResourceBarrier(const D3D12_RESOURCE_BARRIER& barrier)
{
	if (barrier.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
	{
		const D3D12_RESOURCE_TRANSITION_BARRIER& transitionBarrier = barrier.Transition;
		const auto iter = mFinalResourceState.find(transitionBarrier.pResource);
		if (iter != mFinalResourceState.end())
		{
			auto& resourceState = iter->second;
			if (transitionBarrier.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES && !resourceState.SubresourceState.empty())
			{
				for (auto subresourceState : resourceState.SubresourceState)
				{
					if (transitionBarrier.StateAfter != subresourceState.second)
					{
						D3D12_RESOURCE_BARRIER newBarrier = barrier;
						newBarrier.Transition.Subresource = subresourceState.first;
						newBarrier.Transition.StateBefore = subresourceState.second;
						mResourceBarriers.push_back(newBarrier);
					}
				}
			}
			else
			{
				auto finalState = resourceState.GetSubresourceState(transitionBarrier.Subresource);
				if (transitionBarrier.StateAfter != finalState)
				{
					D3D12_RESOURCE_BARRIER newBarrier = barrier;
					newBarrier.Transition.StateBefore = finalState;
					mResourceBarriers.push_back(newBarrier);
				}
			}
		}
		else
		{
			mPendingResourceBarriers.push_back(barrier);
		}

		mFinalResourceState[transitionBarrier.pResource].SetSubresourceState(transitionBarrier.Subresource, transitionBarrier.StateAfter);
	}
	else
	{
		mResourceBarriers.push_back(barrier);
	}
}
void ResourceStateTracker::TransitionResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateAfter, UINT subResource)
{
	if (resource)
	{
		ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(resource, D3D12_RESOURCE_STATE_COMMON, stateAfter, subResource));
	}
}
void ResourceStateTracker::TransitionResource(const Resource& resource, D3D12_RESOURCE_STATES stateAfter, UINT subResource)
{
	TransitionResource(resource.GetD3D12Resource().Get(), stateAfter, subResource);
}
void ResourceStateTracker::UAVBarrier(const Resource* resource)
{
	ID3D12Resource* pResource = resource != nullptr ? resource->GetD3D12Resource().Get() : nullptr;
	ResourceBarrier(CD3DX12_RESOURCE_BARRIER::UAV(pResource));
}
void ResourceStateTracker::AliasBarrier(const Resource* beforeResource, const Resource* afterResource)
{
	ID3D12Resource* pResourceBefore = beforeResource != nullptr ? beforeResource->GetD3D12Resource().Get() : nullptr;
	ID3D12Resource* pResourceAfter = afterResource != nullptr ? afterResource->GetD3D12Resource().Get() : nullptr;
	ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Aliasing(pResourceBefore, pResourceAfter));
}

uint32_t ResourceStateTracker::FlushPendingResourceBarriers(CommandList& commandList)
{
	assert(msIsLocked);
	ResourceBarriers resourceBarriers;
	resourceBarriers.reserve(mPendingResourceBarriers.size());
	for (auto pendingBarrier : mPendingResourceBarriers)
	{
		if (pendingBarrier.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION) 
		{
			auto pendingTransition = pendingBarrier.Transition;

			const auto& iter = msGlobalResourceState.find(pendingTransition.pResource);
			if (iter != msGlobalResourceState.end())
			{
				auto& resourceState = iter->second;
				if (pendingTransition.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES && !resourceState.SubresourceState.empty())
				{
					for (auto subresourceState : resourceState.SubresourceState)
					{
						if (pendingTransition.StateAfter != subresourceState.second)
						{
							D3D12_RESOURCE_BARRIER newBarrier = pendingBarrier;
							newBarrier.Transition.Subresource = subresourceState.first;
							newBarrier.Transition.StateBefore = subresourceState.second;
							resourceBarriers.push_back(newBarrier);
						}
					}
				}
				else
				{
					auto globalState = (iter->second).GetSubresourceState(pendingTransition.Subresource);
					if (pendingTransition.StateAfter != globalState)
					{
						pendingBarrier.Transition.StateBefore = globalState;
						resourceBarriers.push_back(pendingBarrier);
					}
				}
			}
		}
	}
	UINT numBarriers = static_cast<UINT>(resourceBarriers.size());
	if (numBarriers > 0)
	{
		auto d3d12CommandList = commandList.GetGraphicsCommandList();
		d3d12CommandList->ResourceBarrier(numBarriers, resourceBarriers.data());
	}
	mPendingResourceBarriers.clear();

	return numBarriers;
}

void ResourceStateTracker::FlushResourceBarriers(CommandList& commandList)
{
	UINT numBarriers = static_cast<UINT>(mResourceBarriers.size());
	if (numBarriers > 0)
	{
		auto d3d12CommandList = commandList.GetGraphicsCommandList();
		d3d12CommandList->ResourceBarrier(numBarriers, mResourceBarriers.data());
		mResourceBarriers.clear();
	}
}
void ResourceStateTracker::CommitFinalResourceStates()
{
	assert(msIsLocked);
	for (const auto& resourceState : mFinalResourceState)
	{
		msGlobalResourceState[resourceState.first] = resourceState.second;
	}

	mFinalResourceState.clear();
}
void ResourceStateTracker::Reset()
{
	mPendingResourceBarriers.clear();
	mResourceBarriers.clear();
	mFinalResourceState.clear();
}
void ResourceStateTracker::Lock()
{
	msGlobalMutex.lock();
	msIsLocked = true;
}
void ResourceStateTracker::Unlock()
{
	msGlobalMutex.unlock();
	msIsLocked = false;
}
void ResourceStateTracker::AddGlobalResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state)
{
	if (resource != nullptr)
	{
		std::lock_guard<std::mutex> lock(msGlobalMutex);
		msGlobalResourceState[resource].SetSubresourceState(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, state);
	}
}
void ResourceStateTracker::RemoveGlobalResourceState(ID3D12Resource* resource)
{
	if (resource != nullptr)
	{
		std::lock_guard<std::mutex> lock(msGlobalMutex);
		msGlobalResourceState.erase(resource);
	}
}