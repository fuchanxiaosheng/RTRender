#include "RenderTarget.h"

RenderTarget::RenderTarget() : mTextures(AttachmentPoint::NumAttachmentPoints), mSize(0, 0) {}


void RenderTarget::AttachTexture(AttachmentPoint attachmentPoint, const Texture& texture)
{
	mTextures[attachmentPoint] = texture;

	if (texture.GetD3D12Resource())
	{
		auto desc = texture.GetD3D12Resource()->GetDesc();
		mSize.x = static_cast<uint32_t>(desc.Width);
		mSize.y = static_cast<uint32_t>(desc.Height);
	}
}

const Texture& RenderTarget::GetTexture(AttachmentPoint attachmentPoint) const
{
	return mTextures[attachmentPoint];
}

void RenderTarget::Resize(DirectX::XMUINT2 size)
{
	mSize = size;
	for (auto& texture : mTextures)
	{
		texture.Resize(mSize.x, mSize.y);
	}

}
void RenderTarget::Resize(uint32_t width, uint32_t height)
{
	Resize(XMUINT2(width, height));
}

DirectX::XMUINT2 RenderTarget::GetSize() const
{
	return mSize;
}

uint32_t RenderTarget::GetWidth() const
{
	return mSize.x;
}

uint32_t RenderTarget::GetHeight() const
{
	return mSize.y;
}

D3D12_VIEWPORT RenderTarget::GetViewport(DirectX::XMFLOAT2 scale, DirectX::XMFLOAT2 bias, float minDepth, float maxDepth) const
{
	UINT64 width = 0;
	UINT height = 0;

	for (int i = AttachmentPoint::Color0; i <= AttachmentPoint::Color7; ++i)
	{
		const Texture& texture = mTextures[i];
		if (texture.IsValid())
		{
			auto desc = texture.GetD3D12ResourceDesc();
			width = std::max(width, desc.Width);
			height = std::max(height, desc.Height);
		}
	}

	D3D12_VIEWPORT viewport = {
		(width * bias.x),      
		(height * bias.y),     
		(width * scale.x),     
		(height * scale.y),    
		minDepth,              
		maxDepth               
	};

	return viewport;
}


const std::vector<Texture>& RenderTarget::GetTextures() const
{
	return mTextures;
}

D3D12_RT_FORMAT_ARRAY RenderTarget::GetRenderTargetFormats() const
{
	D3D12_RT_FORMAT_ARRAY rtvFormats = {};


	for (int i = AttachmentPoint::Color0; i <= AttachmentPoint::Color7; ++i)
	{
		const Texture& texture = mTextures[i];
		if (texture.IsValid())
		{
			rtvFormats.RTFormats[rtvFormats.NumRenderTargets++] = texture.GetD3D12ResourceDesc().Format;
		}
	}

	return rtvFormats;
}

DXGI_FORMAT RenderTarget::GetDepthStencilFormat() const
{
	DXGI_FORMAT dsvFormat = DXGI_FORMAT_UNKNOWN;
	const Texture& depthStencilTexture = mTextures[AttachmentPoint::DepthStencil];
	if (depthStencilTexture.IsValid())
	{
		dsvFormat = depthStencilTexture.GetD3D12ResourceDesc().Format;
	}

	return dsvFormat;
}