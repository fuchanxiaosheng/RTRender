#ifndef __TEXTURE_H_
#define __TEXTURE_H_

#include "Core.h"
#include "Resource.h"
#include "DescriptorAllocationBlock.h"
#include "TextureUsage.h"

class Texture : public Resource
{
public:
	explicit Texture(TextureUsage textureUsage = TextureUsage::Albedo, const std::wstring & name = L"");
	explicit Texture(const D3D12_RESOURCE_DESC& resourceDesc, const D3D12_CLEAR_VALUE* clearValue = nullptr, TextureUsage textureUsage = TextureUsage::Albedo, const std::wstring& name = L"");
	explicit Texture(ComPtr<ID3D12Resource> resource, TextureUsage textureUsage = TextureUsage::Albedo, const std::wstring& name = L"");

	Texture(const Texture& copy);
	Texture(Texture&& copy);

	Texture& operator=(const Texture& other);
	Texture& operator=(Texture&& other);

	virtual ~Texture();

	TextureUsage GetTextureUsage() const
	{
		return mTextureUsage;
	}

	void SetTextureUsage(TextureUsage textureUsage)
	{
		mTextureUsage = textureUsage;
	}

	void Resize(uint32_t width, uint32_t height, uint32_t depthOrArraySize = 1);

	virtual void CreateViews();
	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc = nullptr) const override;
	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc = nullptr) const override;
	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetRenderTargetView() const;
	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView() const;

	bool CheckSRVSupport()
	{
		return CheckFormatSupport(D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE);
	}

	bool CheckRTVSupport()
	{
		return CheckFormatSupport(D3D12_FORMAT_SUPPORT1_RENDER_TARGET);
	}

	bool CheckUAVSupport()
	{
		return CheckFormatSupport(D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW) && CheckFormatSupport(D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) 
								  && CheckFormatSupport(D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE);
	}

	bool CheckDSVSupport()
	{
		return CheckFormatSupport(D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL);
	}

	static bool IsUAVCompatibleFormat(DXGI_FORMAT format);
	static bool IsSRGBFormat(DXGI_FORMAT format);
	static bool IsBGRFormat(DXGI_FORMAT format);
	static bool IsDepthFormat(DXGI_FORMAT format);
	
	static DXGI_FORMAT GetTypelessFormat(DXGI_FORMAT format);
	static DXGI_FORMAT GetUAVCompatableFormat(DXGI_FORMAT format);

private:
	DescriptorAllocationBlock CreateShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc) const;
	DescriptorAllocationBlock CreateUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc) const;

	mutable std::unordered_map<size_t, DescriptorAllocationBlock> mShaderResourceViews;
	mutable std::unordered_map<size_t, DescriptorAllocationBlock> mUnorderedAccessViews;
	mutable std::mutex mShaderResourceViewsMutex;
	mutable std::mutex mUnorderedAccessViewsMutex;

	DescriptorAllocationBlock mRenderTargetView;
	DescriptorAllocationBlock mDepthStencilView;

	TextureUsage mTextureUsage;
};


#endif
