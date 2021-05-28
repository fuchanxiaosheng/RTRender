#ifndef __GUI_H_
#define __GUI_H_

#include "../ext/imgui/imgui.h"

#include "Core.h"


#include "../ext/imgui/imgui_impl_win32.h"

class CommandList;
class Texture;
class RenderTarget;
class RootSignature;
class Window;

class GUI
{
public:
	GUI();
	virtual ~GUI();

	virtual bool Initialize(std::shared_ptr<Window> window);
	virtual void NewFrame();
	virtual void Render(std::shared_ptr<CommandList> commandList, const RenderTarget& renderTarget);
	virtual void Destroy();

private:
	ImGuiContext* mpImGuiCtx;
	std::unique_ptr<Texture> mFontTexture;
	std::unique_ptr<RootSignature> mRootSignature;
	ComPtr<ID3D12PipelineState> mPipelineState;
	std::shared_ptr<Window> mWindow;
};



#endif
