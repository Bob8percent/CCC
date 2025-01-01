#include <Windows.h>
#include <tchar.h>
#include <combaseapi.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#include <vector>
#ifdef _DEBUG
#include <iostream>
#endif

void debugOutputFormatString(const char* format, ...)
{
#ifdef _DEBUG
	va_list valist;
	va_start(valist, format);
	printf(format, valist);
	va_end(valist);
#endif
}

LRESULT windowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	// �E�B���h�E���j�����ꂽ��I��
	if (msg == WM_DESTROY)
	{
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

bool enableDebugLayer()
{
	ID3D12Debug* debugLayer = nullptr;
	auto res = D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));
	if (FAILED(res))
	{
		return false;
	}
	debugLayer->EnableDebugLayer();
	debugLayer->Release();
	return true;
}

#ifdef _DEBUG
int main()
#else
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
#endif
{
	// �E�B���h�E�̍쐬
	WNDCLASSEX w{};
	w.cbSize = sizeof(WNDCLASSEX);
	w.lpfnWndProc = (WNDPROC)windowProcedure;
	w.lpszClassName = _T("CCCWindow");
	w.hInstance = GetModuleHandle(0);

	RegisterClassEx(&w);
	auto window_width = 1920;
	auto window_height = 1280;

	RECT wrc = { 0,0,window_width, window_height };

	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);
	HWND hwnd = CreateWindow(
		w.lpszClassName,
		_T("CCCEngine"),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		w.hInstance,
		nullptr
	);
	if (!IsWindow(hwnd))
	{
		std::cerr << "Failed to create window.\n";
		return 1;
	}

	// �f�o�b�O���C���[�̗L����
#ifdef _DEBUG
	if (!enableDebugLayer())
	{
		std::cerr << "Failed to enable debug layer.\n";
		return 1;
	}
#endif

	ID3D12Device* _dev = nullptr;
	IDXGIFactory6* _dxgiFactory = nullptr;
	IDXGISwapChain4* _swapchain = nullptr;

	// �f�o�C�X�̍쐬
	D3D_FEATURE_LEVEL levels[]{
		D3D_FEATURE_LEVEL_12_2,
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0
	};
	for (auto level : levels)
	{
		auto res = D3D12CreateDevice(
			nullptr,
			level,
			IID_PPV_ARGS(&_dev)
		);
		if (SUCCEEDED(res))
		{
			break;
		}
	}
	if (_dev == nullptr)
	{
		return 1;
	}

	// �g�p����A�_�v�^�[�̍쐬
#ifdef _DEBUG
	auto res = CreateDXGIFactory2(
		DXGI_CREATE_FACTORY_DEBUG,
		IID_PPV_ARGS(&_dxgiFactory)
	);
#else
	auto res = CreateDXGIFactory1(
		IID_PPV_ARGS(&_dxgiFactory)
	);
#endif
	if (FAILED(res))
	{
		std::cerr << "Failed to create DXGI Factory.\n";
		return 1;
	}
	std::vector<IDXGIAdapter*> adapters;
	IDXGIAdapter* tmpAdapter = nullptr;
	for (auto i = 0;
		_dxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND;
		++i)
	{
		adapters.push_back(tmpAdapter);
	}
	tmpAdapter = nullptr;
	for (auto adapter : adapters)
	{
		DXGI_ADAPTER_DESC adesc{};
		adapter->GetDesc(&adesc);
		std::wstring strDesc = adesc.Description;
		if (strDesc.find(L"NVIDIA") != std::string::npos)
		{
			tmpAdapter = adapter;
			break;
		}
	}
	if (tmpAdapter == nullptr)
	{
		std::cerr << "Can't find NVIDIA GPU.\n";
		return 1;
	}

	// �R�}���h���X�g�̍쐬
	ID3D12CommandAllocator* _cmdAllocator = nullptr;
	ID3D12GraphicsCommandList* _cmdList = nullptr;
	res = _dev->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&_cmdAllocator)
	);
	if (FAILED(res))
	{
		std::cerr << "Failed to create Command Allocator.\n";
		return 1;
	}
	res = _dev->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		_cmdAllocator,
		nullptr,
		IID_PPV_ARGS(&_cmdList)
	);
	if (FAILED(res))
	{
		std::cerr << "Failed to create Command List.\n";
		return 1;
	}
	// �R�}���h�L���[�̍쐬
	ID3D12CommandQueue* _cmdQueue = nullptr;
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc{};
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cmdQueueDesc.NodeMask = 0;
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	res = _dev->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&_cmdQueue));
	if (FAILED(res))
	{
		std::cerr << "Failed to create Command Queue.\n";
		return 1;
	}

	// �X���b�v�`�F�[���̍쐬
	DXGI_SWAP_CHAIN_DESC1 swapchainDesc{};
	swapchainDesc.Width = window_width;
	swapchainDesc.Height = window_height;
	swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchainDesc.Stereo = false;
	swapchainDesc.SampleDesc.Count = 1;
	swapchainDesc.SampleDesc.Quality = 0;
	swapchainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swapchainDesc.BufferCount = 2;
	swapchainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	res = _dxgiFactory->CreateSwapChainForHwnd(
		_cmdQueue,
		hwnd,
		&swapchainDesc,
		nullptr,
		nullptr,
		(IDXGISwapChain1**)&_swapchain
	);
	if (FAILED(res))
	{
		std::cerr << "Failed to create Swap Chain.\n";
		return 1;
	}
	
	// �f�B�X�N���v�^�̍쐬
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ID3D12DescriptorHeap* rtvHeaps = nullptr;
	res = _dev->CreateDescriptorHeap(
		&heapDesc,
		IID_PPV_ARGS(&rtvHeaps)
	);
	if (FAILED(res))
	{
		std::cerr << "Failed to create Descriptor Heap.\n";
		return 1;
	}

	// �r���[���쐬���X���b�v�`�F�[���Ƃ̕R�Â�
	DXGI_SWAP_CHAIN_DESC swpDesc{};
	res = _swapchain->GetDesc(&swpDesc);
	if (FAILED(res))
	{
		std::cerr << "Failed to get Swap Chain Desc.\n";
		return 1;
	}
	std::vector<ID3D12Resource*> _backBuffers(swpDesc.BufferCount);
	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	const auto DS_RTV_SIZE = _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	for (auto i = 0; i < swpDesc.BufferCount; ++i)
	{
		res = _swapchain->GetBuffer(i, IID_PPV_ARGS(&_backBuffers[i]));
		if (FAILED(res))
		{
			std::cerr << "Failed to get back buffer.\n";
			return 1;
		}
		// TODO: �r���[�̎�ނɂ���ăC���N�������g�T�C�Y�̏ꍇ����
		handle.ptr += i * DS_RTV_SIZE;
		_dev->CreateRenderTargetView(_backBuffers[i], nullptr, handle);
	}
	
	// �t�F���X�̍쐬
	ID3D12Fence* _fence = nullptr;
	UINT64 _fenceval = 0;
	res = _dev->CreateFence(
		_fenceval,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&_fence)
	);
	if (FAILED(res))
	{
		std::cerr << "Failed to create fence.\n";
		return 1;
	}

	// �E�B���h�E�\��
	ShowWindow(hwnd, SW_SHOW);

	MSG msg = {};
	while (true) 
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		if (msg.message == WM_QUIT) {
			break;
		}

		// �X���b�v�`�F�[���𓮍삳����
		res = _cmdAllocator->Reset();
		if (SUCCEEDED(res))
		{
			std::cerr << "Failed to reset Command Allocator.\n";
			return 1;
		}
		auto bbIdx = _swapchain->GetCurrentBackBufferIndex();
		auto rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
		rtvH.ptr += bbIdx * DS_RTV_SIZE;
		D3D12_RESOURCE_BARRIER _barrierDesc{};
		_barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		_barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		_barrierDesc.Transition.pResource = _backBuffers[bbIdx];
		_barrierDesc.Transition.Subresource = 0;
		_barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		_barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		_cmdList->ResourceBarrier(1, &_barrierDesc);
		_cmdList->OMSetRenderTargets(1, &rtvH, true, nullptr);
		float clearColor[]{ 1.f,1.f,0.f,1.f };
		_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
		_barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		_barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		_cmdList->ResourceBarrier(1, &_barrierDesc);
		_cmdList->Close();
		ID3D12CommandList* cmdLists[]{ _cmdList };
		_cmdQueue->ExecuteCommandLists(1, cmdLists);
		_cmdQueue->Signal(_fence, ++_fenceval);
		if (_fence->GetCompletedValue() != _fenceval)
		{
			auto event = CreateEvent(nullptr, false, false, nullptr);
			_fence->SetEventOnCompletion(_fenceval, event);
			WaitForSingleObject(event, INFINITE);
			CloseHandle(event);
		}
		_cmdAllocator->Reset();
		_cmdList->Reset(_cmdAllocator, nullptr);
		_swapchain->Present(1, 0);
		ShowWindow(hwnd, SW_SHOW);
	}
	UnregisterClass(w.lpszClassName, w.hInstance);
	return 0;
}