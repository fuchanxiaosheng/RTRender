#include "Game.h"


Game::Game(const std::wstring& name, int width, int height, bool vSync)
	: mName(name), mWidth(width), mHeight(height), mvSync(vSync) {}

Game::~Game()
{
	assert(!mpWindow && "请在销毁Game对象前调用Game::Destroy().");
}

bool Game::Initialize()
{
	if (!DirectX::XMVerifyCPUSupport())
	{
		MessageBoxA(NULL, "验证DirectX Math library支持失败", "Error", MB_OK | MB_ICONERROR);
		return false;
	}

	mpWindow = Application::Get().CreateRenderWindow(mName, mWidth, mHeight, mvSync);
	mpWindow->RegisterCallbacks(shared_from_this());
	mpWindow->Show();

	return true;
}

void Game::Destroy()
{
	Application::Get().DestroyWindow(mpWindow);
	mpWindow.reset();
}

void Game::OnUpdate(UpdateEventArgs& e)
{

}

void Game::OnRender(RenderEventArgs& e)
{

}

void Game::OnKeyPressed(KeyEventArgs& e)
{
}

void Game::OnKeyReleased(KeyEventArgs& e)
{
}

void Game::OnMouseMoved(class MouseMotionEventArgs& e)
{
}

void Game::OnMouseButtonPressed(MouseButtonEventArgs& e)
{
}

void Game::OnMouseButtonReleased(MouseButtonEventArgs& e)
{
}

void Game::OnMouseWheel(MouseWheelEventArgs& e)
{
}

void Game::OnResize(ResizeEventArgs& e)
{
	mWidth = e.Width;
	mHeight = e.Height;
}

void Game::OnWindowDestroy()
{
	UnloadContent();
}

