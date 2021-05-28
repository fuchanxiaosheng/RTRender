#ifndef __WINDOW_H_
#define __WINDOW_H_


#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "Core.h"
#include "Events.h"
#include "HighResolutionClock.h"
#include "Application.h"
#include "CommandQueue.h"
#include "Game.h"
#include "GUI.h"
#include "RenderTarget.h"
#include "Texture.h"

class Game;
class Texture;

class Window : public std::enable_shared_from_this<Window>
{
public:
	static const UINT BufferCount = 3;
	HWND GetWindowHandle() const;
	void InitiaLize();
	void Destroy();

	const std::wstring& GetWindowName() const;

	int GetClientWidth() const;
	int GetClientHeight() const;
	bool IsVSync() const;
	void SetVSync(bool vSync);
	void ToggleVSync();

	bool IsFullScreen() const;

	void SetFullscreen(bool fullscreen);
	void ToggleFullscreen();
	void Show();
	void Hide();
	const RenderTarget& GetRenderTarget() const;
	UINT Present(const Texture& texture = Texture());



protected:
	friend LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
	friend class Application;
	friend class Game;

	Window() = delete;
	Window(HWND hWnd, const std::wstring& windowName, int clientWidth, int clientHeight, bool vSync);
	virtual ~Window();
	void RegisterCallbacks(std::shared_ptr<Game> pGame);
	virtual void OnUpdate(UpdateEventArgs& e);
	virtual void OnRender(RenderEventArgs& e);

	virtual void OnKeyPressed(KeyEventArgs& e);
	virtual void OnKeyReleased(KeyEventArgs& e);
	virtual void OnMouseMoved(MouseMotionEventArgs& e);
	virtual void OnMouseButtonPressed(MouseButtonEventArgs& e);
	virtual void OnMouseButtonReleased(MouseButtonEventArgs& e);
	virtual void OnMouseWheel(MouseWheelEventArgs& e);
	virtual void OnResize(ResizeEventArgs& e);
	Microsoft::WRL::ComPtr<IDXGISwapChain4> CreateSwapChain();
	void UpdateRenderTargetViews();

private:
	Window(const Window& copy) = delete;
	Window& operator=(const Window& other) = delete;

	HWND mhWnd;

	std::wstring mWindowName;

	int mClientWidth;
	int mClientHeight;
	bool mVSync;
	bool mFullscreen;

	HighResolutionClock mUpdateClock;
	HighResolutionClock mRenderClock;
	UINT64 mFenceValues[BufferCount];
	uint64_t mFrameValues[BufferCount];

	std::weak_ptr<Game> mpGame;

	Microsoft::WRL::ComPtr<IDXGISwapChain4> mDxgiSwapChain;
	Texture mBackBufferTextures[BufferCount];
	mutable RenderTarget mRenderTarget;
	
	UINT mCurrentBackBufferIndex;

	RECT mWindowRect;
	bool mIsTearingSupported;

	int mPreviousMouseX;
	int mPreviousMouseY;
	GUI mGUI;

};


#endif
