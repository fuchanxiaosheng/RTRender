#ifndef __APPLICATION_H_
#define __APPLICATION_H_

#include "Core.h"
#include "DescriptorAllocationBlock.h"

class CommandQueue;
class DescriptorAllocator;
class Game;
class Window;

class Application
{
public:
	static void Create(HINSTANCE hInst);
	static void Destroy();
	static Application& Get();
	bool IsTearingSupported() const;
	DXGI_SAMPLE_DESC GetMultisampleQualityLevels(DXGI_FORMAT format, UINT sampleNum, D3D12_MULTISAMPLE_QUALITY_LEVEL_FLAGS flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE) const;
	std::shared_ptr<Window> CreateRenderWindow(const std::wstring& windowName, int clientWidth, int clientHeight, bool vSync = true);
	void DestroyWindow(const std::wstring& windowName);
	void DestroyWindow(std::shared_ptr<Window> window);
	std::shared_ptr<Window> GetWindowByName(const std::wstring& windowName);
	int Run(std::shared_ptr<Game> pGame);
	void Quit(int exitCode = 0);
	ComPtr<ID3D12Device2> GetDevice() const;
	std::shared_ptr<CommandQueue> GetCommandQueue(D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT) const;
	void Flush();
	DescriptorAllocationBlock AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors = 1);
	void ReleaseTheUsedDescriptors(uint64_t finishedFrame);
	ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(UINT numDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE type);
	UINT GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE type) const;

	static uint64_t GetFrameCount()
	{
		return msFrameCount;
	}

protected:
	Application(HINSTANCE hInst);
	virtual ~Application();
	void Initialize();

	ComPtr<IDXGIAdapter4> GetAdapter(bool bUseWarp);
	ComPtr<ID3D12Device2> CreateDevice(ComPtr<IDXGIAdapter4> adapter);
	bool CheckTearingSupport();

private:
	friend LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
	Application(const Application& copy) = delete;
	Application& operator=(const Application& other) = delete;
	HINSTANCE mhInstance;

	ComPtr<ID3D12Device2> mDevice;

	std::shared_ptr<CommandQueue> mDirectCommandQueue;
	std::shared_ptr<CommandQueue> mComputeCommandQueue;
	std::shared_ptr<CommandQueue> mCopyCommandQueue;

	std::unique_ptr<DescriptorAllocator> mDescriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

	bool mTearingSupported;
	static uint64_t msFrameCount;
};

#endif
