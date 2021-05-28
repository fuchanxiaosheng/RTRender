#include "Window.h"


#include "Application.h"
#include "CommandQueue.h"
#include "CommandList.h"
#include "Game.h"
#include "GUI.h"
#include "RenderTarget.h"
#include "ResourceStateTracker.h"
#include "Texture.h"

Window::Window(HWND hWnd, const std::wstring& windowName, int clientWidth, int clientHeight, bool vSync)
	: mhWnd(hWnd), mWindowName(windowName), mClientWidth(clientWidth), mClientHeight(clientHeight), mVSync(vSync), mFullscreen(false), mFenceValues{0}, mFrameValues{0}
{
	Application& app = Application::Get();

	mIsTearingSupported = app.IsTearingSupported();

	for (int i = 0; i < BufferCount; i++)
	{
		mBackBufferTextures[i].SetName(L"BackBuffer[" + std::to_wstring(i) + L"]");
	}
	mDxgiSwapChain = CreateSwapChain();
	UpdateRenderTargetViews();
}

Window::~Window()
{
	assert(!mhWnd && "请在Application::DestroyWindow调用前销毁Window");
}

void Window::InitiaLize()
{
	mGUI.Initialize(shared_from_this());
}

HWND Window::GetWindowHandle() const
{
	return mhWnd;
}

const std::wstring& Window::GetWindowName() const
{
	return mWindowName;
}

void Window::Show()
{
	ShowWindow(mhWnd, SW_SHOW);
}

void Window::Hide()
{
	ShowWindow(mhWnd, SW_HIDE);
}

void Window::Destroy()
{
	if (auto pGame = mpGame.lock())
	{
		pGame->OnWindowDestroy();
	}

	if (mhWnd)
	{
		DestroyWindow(mhWnd);
		mhWnd = nullptr;
	}
}

int Window::GetClientWidth() const
{
	return mClientWidth;
}

int Window::GetClientHeight() const
{
	return mClientHeight;
}

bool Window::IsVSync() const
{
	return mVSync;
}

void Window::SetVSync(bool vSync)
{
	mVSync = vSync;
}

void Window::ToggleVSync()
{
	SetVSync(!mVSync);
}

bool Window::IsFullScreen() const
{
	return mFullscreen;
}

void Window::SetFullscreen(bool fullscreen)
{
	if (mFullscreen != fullscreen)
	{
		mFullscreen = fullscreen;

		if (mFullscreen) 
		{
			GetWindowRect(mhWnd, &mWindowRect);
			UINT windowStyle = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);

			SetWindowLongW(mhWnd, GWL_STYLE, windowStyle);
			HMONITOR hMonitor = ::MonitorFromWindow(mhWnd, MONITOR_DEFAULTTONEAREST);
			MONITORINFOEX monitorInfo = {};
			monitorInfo.cbSize = sizeof(MONITORINFOEX);
			GetMonitorInfo(hMonitor, &monitorInfo);

			SetWindowPos(mhWnd, HWND_TOP, monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top, monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
						 monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top, SWP_FRAMECHANGED | SWP_NOACTIVATE);

			ShowWindow(mhWnd, SW_MAXIMIZE);
		}
		else
		{
			SetWindowLong(mhWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);

			SetWindowPos(mhWnd, HWND_NOTOPMOST, mWindowRect.left, mWindowRect.top, mWindowRect.right - mWindowRect.left, mWindowRect.bottom - mWindowRect.top, SWP_FRAMECHANGED | SWP_NOACTIVATE);

			ShowWindow(mhWnd, SW_NORMAL);
		}
	}
}

void Window::ToggleFullscreen()
{
	SetFullscreen(!mFullscreen);
}


void Window::RegisterCallbacks(std::shared_ptr<Game> pGame)
{
	mpGame = pGame;

	return;
}

void Window::OnUpdate(UpdateEventArgs&)
{
	mGUI.NewFrame();
	mUpdateClock.Tick();

	if (auto pGame = mpGame.lock())
	{
		UpdateEventArgs updateEventArgs(mUpdateClock.GetDeltaSeconds(), mUpdateClock.GetTotalSeconds());
		pGame->OnUpdate(updateEventArgs);
	}
}

void Window::OnRender(RenderEventArgs&)
{
	mRenderClock.Tick();

	if (auto pGame = mpGame.lock())
	{
		RenderEventArgs renderEventArgs(mRenderClock.GetDeltaSeconds(), mRenderClock.GetTotalSeconds());
		pGame->OnRender(renderEventArgs);
	}
}

void Window::OnKeyPressed(KeyEventArgs& e)
{
	if (auto pGame = mpGame.lock())
	{
		pGame->OnKeyPressed(e);
	}
}

void Window::OnKeyReleased(KeyEventArgs& e)
{
	if (auto pGame = mpGame.lock())
	{
		pGame->OnKeyReleased(e);
	}
}

void Window::OnMouseMoved(MouseMotionEventArgs& e)
{
	e.RelX = e.X - mPreviousMouseX;
	e.RelY = e.Y - mPreviousMouseY;

	mPreviousMouseX = e.X;
	mPreviousMouseY = e.Y;
	if (auto pGame = mpGame.lock())
	{
		pGame->OnMouseMoved(e);
	}
}

void Window::OnMouseButtonPressed(MouseButtonEventArgs& e)
{
	mPreviousMouseX = e.X;
	mPreviousMouseY = e.Y;
	if (auto pGame = mpGame.lock())
	{
		pGame->OnMouseButtonPressed(e);
	}
}

void Window::OnMouseButtonReleased(MouseButtonEventArgs& e)
{
	if (auto pGame = mpGame.lock())
	{
		pGame->OnMouseButtonReleased(e);
	}
}

void Window::OnMouseWheel(MouseWheelEventArgs& e)
{
	if (auto pGame = mpGame.lock())
	{
		pGame->OnMouseWheel(e);
	}
}

void Window::OnResize(ResizeEventArgs& e)
{
	if (mClientWidth != e.Width || mClientHeight != e.Height)
	{
		mClientWidth = std::max(1, e.Width);
		mClientHeight = std::max(1, e.Height);

		Application::Get().Flush();

		mRenderTarget.AttachTexture(Color0, Texture());
		for (int i = 0; i < BufferCount; ++i)
		{
			ResourceStateTracker::RemoveGlobalResourceState(mBackBufferTextures[i].GetD3D12Resource().Get());
			mBackBufferTextures[i].Reset();
		}

		DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
		ThrowIfFailed(mDxgiSwapChain->GetDesc(&swapChainDesc));
		ThrowIfFailed(mDxgiSwapChain->ResizeBuffers(BufferCount, mClientWidth, mClientHeight, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

		mCurrentBackBufferIndex = mDxgiSwapChain->GetCurrentBackBufferIndex();

		UpdateRenderTargetViews();
	}

	if (auto pGame = mpGame.lock())
	{
		pGame->OnResize(e);
	}
}

Microsoft::WRL::ComPtr<IDXGISwapChain4> Window::CreateSwapChain()
{
	Application& app = Application::Get();

	ComPtr<IDXGISwapChain4> dxgiSwapChain4;
	ComPtr<IDXGIFactory4> dxgiFactory4;
	UINT createFactoryFlags = 0;
#if defined(_DEBUG)
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)));

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = mClientWidth;
	swapChainDesc.Height = mClientHeight;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.SampleDesc = { 1, 0 };
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = BufferCount;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapChainDesc.Flags = mIsTearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
	ID3D12CommandQueue* pCommandQueue = app.GetCommandQueue()->GetD3D12CommandQueue().Get();

	ComPtr<IDXGISwapChain1> swapChain1;
	ThrowIfFailed(dxgiFactory4->CreateSwapChainForHwnd(pCommandQueue, mhWnd, &swapChainDesc, nullptr, nullptr, &swapChain1));

	ThrowIfFailed(dxgiFactory4->MakeWindowAssociation(mhWnd, DXGI_MWA_NO_ALT_ENTER));

	ThrowIfFailed(swapChain1.As(&dxgiSwapChain4));

	mCurrentBackBufferIndex = dxgiSwapChain4->GetCurrentBackBufferIndex();

	return dxgiSwapChain4;
}

void Window::UpdateRenderTargetViews()
{
	for (int i = 0; i < BufferCount; i++)
	{
		ComPtr<ID3D12Resource> backBuffer;
		ThrowIfFailed(mDxgiSwapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));
		ResourceStateTracker::AddGlobalResourceState(backBuffer.Get(), D3D12_RESOURCE_STATE_COMMON);
		mBackBufferTextures[i].SetResource(backBuffer);
		mBackBufferTextures[i].CreateViews();
	}
}

const RenderTarget& Window::GetRenderTarget() const
{
	mRenderTarget.AttachTexture(AttachmentPoint::Color0, mBackBufferTextures[mCurrentBackBufferIndex]);
	return mRenderTarget;
}


UINT Window::Present(const Texture& texture)
{
	auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto commandList = commandQueue->GetCommandList();

	auto& backBuffer = mBackBufferTextures[mCurrentBackBufferIndex];
	if (texture.IsValid())
	{
		if (texture.GetD3D12ResourceDesc().SampleDesc.Count > 1)
		{
			commandList->ResolveSubresource(backBuffer, texture);
		}
		else
		{
			commandList->CopyResource(backBuffer, texture);
		}
	}

	RenderTarget renderTarget;
	renderTarget.AttachTexture(AttachmentPoint::Color0, backBuffer);
	mGUI.Render(commandList, renderTarget);
	commandList->TransitionBarrier(backBuffer, D3D12_RESOURCE_STATE_PRESENT);
	commandQueue->ExecuteCommandList(commandList);
	UINT syncInterval = mVSync ? 1 : 0;
	UINT presentFlags = mIsTearingSupported && !mVSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
	ThrowIfFailed(mDxgiSwapChain->Present(syncInterval, presentFlags));
	mFenceValues[mCurrentBackBufferIndex] = commandQueue->Signal();
	mFrameValues[mCurrentBackBufferIndex] = Application::GetFrameCount();
	mCurrentBackBufferIndex = mDxgiSwapChain->GetCurrentBackBufferIndex();
	commandQueue->WaitForFenceValue(mFenceValues[mCurrentBackBufferIndex]);
	Application::Get().ReleaseTheUsedDescriptors(mFrameValues[mCurrentBackBufferIndex]);

	return mCurrentBackBufferIndex;
}
