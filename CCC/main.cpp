#include <Windows.h>
#include <tchar.h>
#include <combaseapi.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#include <vector>
#ifdef _DEBUG
#include <iostream>
#endif

using namespace DirectX;

void debugOutputFormatString(const char* format, ...)
{
#ifdef _DEBUG
	va_list valist;
	va_start(valist, format);
	printf(format, valist);
	va_end(valist);
#endif
}

void printBlobErrorMsg(HRESULT res, ID3DBlob* errorBlob)
{
#ifdef _DEBUG
	if (res == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
	{
		::OutputDebugStringA("ファイルが見つからない");
	}
	else
	{
		std::string errorStr;
		errorStr.resize(errorBlob->GetBufferSize());
		std::copy_n((char*)errorBlob->GetBufferPointer(),
			errorBlob->GetBufferSize(),
			errorStr.begin());
		errorStr += "\n";
		::OutputDebugStringA(errorStr.c_str());
	}
#endif
}

LRESULT windowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	// ウィンドウが破棄されたら終了
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
	// ウィンドウの作成
	WNDCLASSEX w{};
	w.cbSize = sizeof(WNDCLASSEX);
	w.lpfnWndProc = (WNDPROC)windowProcedure;
	w.lpszClassName = _T("CCCWindow");
	w.hInstance = GetModuleHandle(0);

	RegisterClassEx(&w);
	auto window_width = 1280;
	auto window_height = 720;

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

	// デバッグレイヤーの有効化
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

	// デバイスの作成
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

	// 使用するアダプターの作成
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

	// コマンドリストの作成
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
	// コマンドキューの作成
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

	// スワップチェーンの作成
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

	// ビューポートの作成
	D3D12_VIEWPORT viewport{};
	viewport.Width = window_width;
	viewport.Height = window_height;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MaxDepth = 1.f;
	viewport.MinDepth = 0.f;

	// シザー矩形の作成
	D3D12_RECT scissorrect{};
	scissorrect.top = 0;
	scissorrect.left = 0;
	scissorrect.right = scissorrect.left + window_width;
	scissorrect.bottom = scissorrect.top + window_height;

	// 頂点バッファの作成
	XMFLOAT3 vertices[]{
		{-0.4f,-0.7f,0.0f} ,
		{-0.4f,0.7f,0.0f} ,
		{0.4f,-0.7f,0.0f} ,
		{0.4f,0.7f,0.0f} ,
	};
	unsigned short indices[]{
		0,1,2,
		2,1,3
	};
	D3D12_HEAP_PROPERTIES heapprop{};
	heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	D3D12_RESOURCE_DESC resDesc{};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Width = sizeof(vertices);
	resDesc.Height = 1;
	resDesc.DepthOrArraySize = 1;
	resDesc.MipLevels = 1;
	resDesc.Format = DXGI_FORMAT_UNKNOWN;
	resDesc.SampleDesc.Count = 1;
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ID3D12Resource* vertbuffer = nullptr;
	res = _dev->CreateCommittedResource(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertbuffer)
	);
	if (FAILED(res))
	{
		std::cerr << "Failed to create vertex buffer.\n";
		return 1;
	}
	XMFLOAT3* vertMap = nullptr;
	res = vertbuffer->Map(0, nullptr, (void**)&vertMap);
	std::copy(std::begin(vertices), std::end(vertices), vertMap);
	vertbuffer->Unmap(0, nullptr);
	D3D12_VERTEX_BUFFER_VIEW vbView{};
	vbView.BufferLocation = vertbuffer->GetGPUVirtualAddress();
	vbView.SizeInBytes = sizeof(vertices);
	vbView.StrideInBytes = sizeof(vertices[0]);
	ID3D12Resource* idxbuffer = nullptr;
	resDesc.Width = sizeof(indices);
	res = _dev->CreateCommittedResource(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&idxbuffer)
	);
	if (FAILED(res))
	{
		std::cerr << "Failed to create index buffer.\n";
		
	}
	unsigned short* mappedidx = nullptr;
	idxbuffer->Map(0, nullptr, (void**)&mappedidx);
	std::copy(std::begin(indices), std::end(indices), mappedidx);
	idxbuffer->Unmap(0, nullptr);
	D3D12_INDEX_BUFFER_VIEW ibView{};
	ibView.BufferLocation = idxbuffer->GetGPUVirtualAddress();
	ibView.Format = DXGI_FORMAT_R16_UINT;
	ibView.SizeInBytes = sizeof(indices);

	// シェーダーオブジェクトの作成
	ID3DBlob* vsBlob = nullptr;
	ID3DBlob* psBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	res = D3DCompileFromFile(
		L"BasicVS.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicVS", "vs_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&vsBlob,
		&errorBlob
	);
	if (FAILED(res))
	{
		std::cerr << "Failed to compile vertex shader.\n";
		printBlobErrorMsg(res, errorBlob);
		return 1;
	}
	res = D3DCompileFromFile(
		L"BasicPS.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicPS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&psBlob,
		&errorBlob
	);
	if (FAILED(res))
	{
		std::cerr << "Failed to compile pixel shader.\n";
		printBlobErrorMsg(res, errorBlob);
		return 1;
	}

	// 頂点レイアウトの作成
	D3D12_INPUT_ELEMENT_DESC inputLayout[]{
		{
			"POSITION",0, DXGI_FORMAT_R32G32B32_FLOAT,0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0
		}
	};

	// ルートシグネチャの作成
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	ID3DBlob* rootSigBlob = nullptr;
	res = D3D12SerializeRootSignature(
		&rootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		&rootSigBlob,
		&errorBlob
	);
	if (FAILED(res))
	{
		std::cerr << "Failed to serialize root signature.\n";
		return 1;
	}
	ID3D12RootSignature* _rootSignature = nullptr;
	res = _dev->CreateRootSignature(
		0,
		rootSigBlob->GetBufferPointer(),
		rootSigBlob->GetBufferSize(),
		IID_PPV_ARGS(&_rootSignature)
	);
	if (FAILED(res))
	{
		std::cerr << "Failed to create root signature.\n";
		return 1;
	}

	// グラフィクスパイプラインステートの作成
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline{};
	gpipeline.pRootSignature = _rootSignature;
	gpipeline.VS.pShaderBytecode = vsBlob->GetBufferPointer();
	gpipeline.VS.BytecodeLength = vsBlob->GetBufferSize();
	gpipeline.PS.pShaderBytecode = psBlob->GetBufferPointer();
	gpipeline.PS.BytecodeLength = psBlob->GetBufferSize();
	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	gpipeline.RasterizerState.MultisampleEnable = false;
	gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	gpipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	gpipeline.RasterizerState.DepthClipEnable = true;
	gpipeline.BlendState.AlphaToCoverageEnable = false;
	gpipeline.BlendState.IndependentBlendEnable = false;
	D3D12_RENDER_TARGET_BLEND_DESC rtBlendDesc;
	rtBlendDesc.BlendEnable = false;
	rtBlendDesc.LogicOpEnable = false;
	rtBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	gpipeline.BlendState.RenderTarget[0] = rtBlendDesc;
	gpipeline.InputLayout.pInputElementDescs = inputLayout;
	gpipeline.InputLayout.NumElements = _countof(inputLayout);
	gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	gpipeline.NumRenderTargets = 1;
	gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gpipeline.SampleDesc.Count = 1;
	gpipeline.SampleDesc.Quality = 0;
	ID3D12PipelineState* _pipelinestate = nullptr;
	res = _dev->CreateGraphicsPipelineState(
		&gpipeline,
		IID_PPV_ARGS(&_pipelinestate)
	);
	if (FAILED(res))
	{
		std::cerr << "Failed to create pipeline state.\n";
		return 1;
	}

	// ディスクリプタの作成
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

	// ビューを作成しスワップチェーンとの紐づけ
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
		// TODO: ビューの種類によってインクリメントサイズの場合分け
		handle.ptr += i * DS_RTV_SIZE;
		_dev->CreateRenderTargetView(_backBuffers[i], nullptr, handle);
	}
	
	// フェンスの作成
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

	// ウィンドウ表示
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

		// コマンドキューの実行
		auto bbIdx = _swapchain->GetCurrentBackBufferIndex();
		D3D12_RESOURCE_BARRIER _barrierDesc{};
		_barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		_barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		_barrierDesc.Transition.pResource = _backBuffers[bbIdx];
		_barrierDesc.Transition.Subresource = 0;
		_barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		_barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		_cmdList->ResourceBarrier(1, &_barrierDesc);
		_cmdList->SetPipelineState(_pipelinestate);
		auto rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
		rtvH.ptr += bbIdx * DS_RTV_SIZE;
		_cmdList->OMSetRenderTargets(1, &rtvH, true, nullptr);
		float clearColor[]{ 1.f,1.f,0.f,1.f };
		_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
		_cmdList->RSSetViewports(1, &viewport);
		_cmdList->RSSetScissorRects(1, &scissorrect);
		_cmdList->SetGraphicsRootSignature(_rootSignature);
		_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		_cmdList->IASetVertexBuffers(0, 1, &vbView);
		_cmdList->IASetIndexBuffer(&ibView);
		_cmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);
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
		_cmdList->Reset(_cmdAllocator, _pipelinestate);
		_swapchain->Present(1, 0);
	}
	UnregisterClass(w.lpszClassName, w.hInstance);
	return 0;
}