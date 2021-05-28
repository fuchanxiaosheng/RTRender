#ifndef __RESOURCE_H_
#define __RESOURCE_H_

#include "Core.h"


class Resource
{
public:
	explicit Resource(const std::wstring& name = L"");
	explicit Resource(const D3D12_RESOURCE_DESC& resourceDesc, const D3D12_CLEAR_VALUE* clearValue = nullptr, const std::wstring& name = L"");
	explicit Resource(ComPtr<ID3D12Resource> resource, const std::wstring& name = L"");

	Resource(const Resource& copy);
	Resource(Resource&& copy);

	Resource& operator=(const Resource& other);
	Resource& operator=(Resource&& other) noexcept;
	virtual ~Resource();

	bool IsValid() const
	{
		return (mResource != nullptr);
	}


	ComPtr<ID3D12Resource> GetD3D12Resource() const
	{
		return mResource;
	}

	D3D12_RESOURCE_DESC GetD3D12ResourceDesc() const
	{
		D3D12_RESOURCE_DESC resDesc = {};
		if (mResource)
		{
			resDesc = mResource->GetDesc();
		}
		return resDesc;
	}

	virtual void SetResource(ComPtr<ID3D12Resource> resource, const D3D12_CLEAR_VALUE* clearValue = nullptr);
	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srcDesc = nullptr) const = 0;
	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc = nullptr) const = 0;


	void SetName(const std::wstring& name);
	virtual void Reset();

	bool CheckFormatSupport(D3D12_FORMAT_SUPPORT1 formatSupport) const;
	bool CheckFormatSupport(D3D12_FORMAT_SUPPORT2 formatSupport) const;

protected:
	
	ComPtr<ID3D12Resource> mResource;
	D3D12_FEATURE_DATA_FORMAT_SUPPORT mFormatSupport;
	std::unique_ptr<D3D12_CLEAR_VALUE> mClearValue;
	std::wstring mResourceName;

private:
	void CheckFeatureSupport();

};

#endif
