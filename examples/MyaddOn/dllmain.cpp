
#include <reshade.hpp>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <windows.h>

#include <wrl.h>			

#pragma comment(lib, "dxgi.lib")

#include "rendercommon.h"

#include "Dx12Present.h"



using namespace Microsoft::WRL;

HINSTANCE g_hInstance;
HWND g_hWnd = NULL;


const reshade::api::format  rsFormat = reshade::api::format::r8g8b8a8_unorm;

struct __declspec(uuid("2FA5FB3D-7873-4E67-9DDA-5D449DB2CB47")) SBSRenderData
{

	//Shader resource view for game render target, as need to render it into buffer
	reshade::api::resource_view RTV_SRV[MAX_BACKBUF_COUNT] = {0};

	reshade::api::resource_view RTV[MAX_BACKBUF_COUNT] = { 0 };

	reshade::api::swapchain *pOurswapchain = {0};

	Dx12Present	dx12Present;

	bool	gameSRVCreated = { false };

};


// 窗口过程
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CLOSE:
		PostQuitMessage(0);
		return 0;
	case WM_DESTROY:
		DestroyWindow(hwnd);
		return 0;
	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}

// 创建窗口的函数
HWND CreateWindowInDLL()
{
	// 注册窗口类
	WNDCLASS wc = {};
	wc.style = CS_GLOBALCLASS;
	wc.lpfnWndProc = WindowProc;  // 窗口过程
	wc.hInstance = g_hInstance;
	wc.lpszClassName = L"MyDLLWindowClass";
	wc.hbrBackground = (HBRUSH)COLOR_BACKGROUND;

	if (!RegisterClass(&wc))
	{
		MessageBoxA(NULL, "Error register Window class", "Error", MB_OK);
		return NULL;
	}

	// 创建窗口
	g_hWnd = CreateWindowEx(
		0,                             // 扩展风格
		L"MyDLLWindowClass",            // 类名
		L"SBS output",                  // 窗口标题
		0,//WS_OVERLAPPEDWINDOW,           // 窗口风格
		CW_USEDEFAULT, CW_USEDEFAULT,  // 初始位置
		WIDTH_D, HEIGHT,                      // 窗口大小
		NULL, NULL, g_hInstance, NULL  // 父窗口、菜单、实例句柄、创建参数
	);

	if (g_hWnd == NULL)
	{
		MessageBoxA(NULL, "Fail to createWindow", "error", MB_OK);
		return NULL;
	}

	// 显示窗口
	ShowWindow(g_hWnd, SW_SHOWNORMAL);

	//Move it to bottom
	SetWindowPos(g_hWnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

	UpdateWindow(g_hWnd);

	return g_hWnd;
}

DWORD WINAPI WindowThreadProc(LPVOID lpParam)
{
	// 创建窗口
	HWND hwnd = CreateWindowInDLL();

	// 启动消息循环
	MSG msg;
	while (GetMessage(&msg, hwnd, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}




/*
// 插件初始化函数
void InitializeSwapChain() {
	// 获取 ReShade 的设备上下文
	g_device = reinterpret_cast<ID3D12Device *>(reshade::api::device());
	g_commandQueue = reinterpret_cast<ID3D12CommandQueue *>(reshade::api::context());

	// 获取 DXGI Factory
	CreateDXGIFactory1(IID_PPV_ARGS(&g_dxgiFactory));

	// 创建交换链描述
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = 1920;    // 目标显示器的宽度
	swapChainDesc.Height = 1080;   // 目标显示器的高度
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;  // 双缓冲
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapChainDesc.Flags = 0;

	// 获取第二显示器的窗口句柄
	g_windowHandle = GetOtherWindowHandle();

	// 创建交换链
	IDXGISwapChain1 *swapChain1 = nullptr;
	g_dxgiFactory->CreateSwapChainForHwnd(g_commandQueue, g_windowHandle, &swapChainDesc, nullptr, nullptr, &swapChain1);
	swapChain1->QueryInterface(IID_PPV_ARGS(&g_swapChain));

	// 创建渲染目标视图(RTV)
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.NumDescriptors = 1;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	g_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&g_rtvHeap));

	// 获取交换链的后台缓冲
	g_swapChain->GetBuffer(0, IID_PPV_ARGS(&g_backBuffer));

	// 创建渲染目标视图
	g_device->CreateRenderTargetView(g_backBuffer, nullptr, g_rtvHeap->GetCPUDescriptorHandleForHeapStart());
}

// 渲染到新的交换链
void RenderToSwapChain() {
	// 在新的交换链上渲染
	ID3D12GraphicsCommandList *commandList = nullptr;
	g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, nullptr, nullptr, IID_PPV_ARGS(&commandList));

	// 设置渲染目标
	commandList->OMSetRenderTargets(1, &g_rtvHeap->GetCPUDescriptorHandleForHeapStart(), FALSE, nullptr);

	// 清除背景
	commandList->ClearRenderTargetView(g_rtvHeap->GetCPUDescriptorHandleForHeapStart(), { 0.0f, 0.0f, 0.0f, 1.0f }, 0, nullptr);

	// 提交渲染命令
	commandList->Close();
	ID3D12CommandQueue *commandQueue = nullptr;
	g_device->GetCommandQueue(0, IID_PPV_ARGS(&commandQueue));
	commandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList *const *>(&commandList));

	// Present到新的交换链
	g_swapChain->Present(1, 0);
}

// 插件卸载时清理资源
void CleanupSwapChain() {
	if (g_swapChain) {
		g_swapChain->Release();
		g_swapChain = nullptr;
	}
	if (g_dxgiFactory) {
		g_dxgiFactory->Release();
		g_dxgiFactory = nullptr;
	}
	if (g_rtvHeap) {
		g_rtvHeap->Release();
		g_rtvHeap = nullptr;
	}
	if (g_backBuffer) {
		g_backBuffer->Release();
		g_backBuffer = nullptr;
	}
}

// 插件生命周期管理
extern "C" __declspec(dllexport) void CALLBACK reshade_init()
{
	InitializeSwapChain();
}

extern "C" __declspec(dllexport) void CALLBACK reshade_render()
{
	RenderToSwapChain();
}

extern "C" __declspec(dllexport) void CALLBACK reshade_cleanup()
{
	CleanupSwapChain();
}

*/

int create_swapchain(HWND hwnd);

static reshade::api::device *g_device = nullptr;

void on_init(reshade::api::device *device)
{
	g_device = device;

	while (g_hWnd == NULL)	
		Sleep(10);

	//创建私有数据
	device->create_private_data<SBSRenderData>();

	SBSRenderData &devData = device->get_private_data <SBSRenderData>();

	//create_swapchain(g_hWnd);

	ID3D12Device *d3d12_device = ((ID3D12Device *)g_device->get_native());

	devData.dx12Present.init_resource(d3d12_device,g_hWnd);
}

static IDXGISwapChain3 *g_swapchain = nullptr;


// Function to retrieve IDXGIFactory from ID3D12Device
void GetDXGIFactoryFromD3D12Device(ID3D12Device *d3d12Device, IDXGIFactory4 **dxgiFactory)
{
	// Step 1: Query IDXGIAdapter from the ID3D12Device
	ComPtr<IDXGIAdapter> dxgiAdapter;
	LUID adapterLuid = d3d12Device->GetAdapterLuid(); // Get the adapter's LUID

	// Use DXGI to enumerate the adapters and match the LUID
	ComPtr<IDXGIFactory4> factory;
	HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
	if (FAILED(hr))
	{
		//throw std::runtime_error("Failed to create DXGIFactory.");
	}

	ComPtr<IDXGIAdapter> matchedAdapter;
	for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != factory->EnumAdapters(adapterIndex, &dxgiAdapter); ++adapterIndex)
	{
		DXGI_ADAPTER_DESC desc;
		dxgiAdapter->GetDesc(&desc);

		if (desc.AdapterLuid.LowPart == adapterLuid.LowPart && desc.AdapterLuid.HighPart == adapterLuid.HighPart)
		{
			matchedAdapter = dxgiAdapter;
			break;
		}
	}

	if (!matchedAdapter)
	{
		//throw std::runtime_error("Failed to match IDXGIAdapter for ID3D12Device.");
	}

	// Step 2: Return the factory
	*dxgiFactory = factory.Detach();
}

int create_swapchain(HWND hwnd)
{

	HRESULT hResult;

	if (g_device == nullptr)
		return -1;

	// 获取 Direct3D 12 设备和命令队列	

	ID3D12Device *d3d12_device = ((ID3D12Device *)g_device->get_native());
	if (!d3d12_device) {
		reshade::log::message(reshade::log::level::info, "Couldn't get a device");
		return -1;
	}	

	ID3D12CommandAllocator *command_allocator;
	d3d12_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocator));
	if (command_allocator == nullptr) {
		reshade::log::message(reshade::log::level::info, "Couldn't ceate command allocator");
		return -1;
	}

	// Describe and create the command queue.
	//ID3D12CommandQueue *command_queue;
	ComPtr<ID3D12CommandQueue>			pIMainCmdQueue;

	D3D12_COMMAND_QUEUE_DESC queue_desc = {};
	queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	d3d12_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&pIMainCmdQueue));
	if (pIMainCmdQueue == nullptr)
	{
		reshade::log::message(reshade::log::level::info, "Couldn't create command queue");
		return -1;
	}

	// 创建 DXGI 工厂
	IDXGIFactory4 *dxgi_factory = nullptr;
	//CreateDXGIFactory1(__uuidof(IDXGIFactory4), reinterpret_cast<void **>(&dxgi_factory));

	GetDXGIFactoryFromD3D12Device(d3d12_device, &dxgi_factory);


	// 设置 SwapChain 描述
	DXGI_SWAP_CHAIN_DESC1 swapchain_desc = {};
	swapchain_desc.BufferCount = MAX_BACKBUF_COUNT;
	swapchain_desc.Width = WIDTH_D; // 分辨率宽度
	swapchain_desc.Height = HEIGHT; // 分辨率高度
	swapchain_desc.Format = RTVFormat; // 颜色格式
	swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // DX12 推荐的交换效果
	swapchain_desc.SampleDesc.Count = 1; // 无多重采样


	// 创建 SwapChain
	IDXGISwapChain1 *swapchain1 = nullptr;
	hResult = dxgi_factory->CreateSwapChainForHwnd(
		pIMainCmdQueue.Get(), // 必须使用命令队列
		hwnd,
		&swapchain_desc,
		nullptr, // 可选：窗口全屏描述
		nullptr, // 可选：输出限制
		&swapchain1);


	// 升级为 IDXGISwapChain3 接口
	swapchain1->QueryInterface(__uuidof(IDXGISwapChain3), reinterpret_cast<void **>(&g_swapchain));

	// 禁用 ALT+ENTER 全屏切换
	dxgi_factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

	// 释放临时资源
	swapchain1->Release();
	dxgi_factory->Release();

	return 0;
}


void register_swapchain_to_reshade()
{
	if (g_swapchain != nullptr && g_device != nullptr)
	{
		//reshade::api::swapchain_desc desc = {};
		//desc.type = reshade::api::swapchain_type::d3d12_swapchain;
		//desc.native_handle = g_swapchain;

		//reshade::register_swapchain(g_swapchain, desc);
	}
}

static void on_present(reshade::api::command_queue *, reshade::api::swapchain *swapChain, const reshade::api::rect *, const reshade::api::rect *, uint32_t, const reshade::api::rect *)
{
	ID3D12Device *device = nullptr;
	reshade::api::device *dev = nullptr;

	dev = swapChain->get_device();

	SBSRenderData& devData  = dev->get_private_data <SBSRenderData>();

	device = ((ID3D12Device *)dev->get_native());

	if (!devData.gameSRVCreated)
	{
		reshade::api::resource_desc desc = dev->get_resource_desc(swapChain->get_current_back_buffer());

		const int maxbackbuf = 5;
		ID3D12Resource *pbackbuf[maxbackbuf];

		int numbackbuf = swapChain->get_back_buffer_count();

		for (int i = 0; i < numbackbuf; i++)
		{
			reshade::api::resource d3dres = swapChain->get_back_buffer(i);			
			
			pbackbuf[i] = (ID3D12Resource*)(d3dres.handle);
		}

		devData.dx12Present.CreateSRV_forGameRTV(device,(DXGI_FORMAT)desc.texture.format,numbackbuf , pbackbuf);
	}


	devData.dx12Present.on_present(swapChain->get_current_back_buffer_index());

	//uint32_t index = swapChain->get_current_back_buffer_index();

	/*
	if (devData.RTV_SRV[index] == NULL)
	{
		reshade::api::resource_view_desc  srv_desc(reshade::api::format::r8g8b8a8_unorm);
		bool bRet = dev->create_resource_view(swapChain->get_back_buffer(index), reshade::api::resource_usage::shader_resource, srv_desc, &devData.RTV_SRV[index]);

		if (!bRet)
			reshade::log::message(reshade::log::level::info, "Couldn't create SRV for render target!");
	}
	else
	*/
	{
		//1. set PSO

		//Set resource view

		//Drawcall
	}





	/*
	// 创建渲染目标视图(RTV)
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.NumDescriptors = 1;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	g_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&g_rtvHeap));

	// 获取交换链的后台缓冲
	g_swapChain->GetBuffer(0, IID_PPV_ARGS(&g_backBuffer));

	// 创建渲染目标视图
	g_device->CreateRenderTargetView(g_backBuffer, nullptr, g_rtvHeap->GetCPUDescriptorHandleForHeapStart());
	}*/


}


static void on_destroy(reshade::api::device *device)
{
	device->destroy_private_data<SBSRenderData>();
}


static void on_init_swapchain(reshade::api::swapchain *swapchain)
{

	reshade::api::device* dev = swapchain->get_device();

	SBSRenderData &devData = dev->get_private_data <SBSRenderData>();

	if (devData.pOurswapchain == NULL)
	{
		devData.pOurswapchain = (reshade::api::swapchain*)-1;
		//create_swapchain(g_hWnd);
	}


	//This is the swap chain we created!
	if (swapchain->get_hwnd() == g_hWnd)
	{


		devData.pOurswapchain = swapchain;

		for (uint32_t i = 0; i < swapchain->get_back_buffer_count(); ++i)
		{
			reshade::api::resource bakbuf;
			//save the render target view for each back buffer
			bakbuf = swapchain->get_back_buffer(i);

			reshade::api::resource_view_desc  rtv_desc(rsFormat);
			bool bRet = dev->create_resource_view(bakbuf, reshade::api::resource_usage::present, rtv_desc, &devData.RTV[i]);

			//devData.RTV[i]
		}
	}
}



extern "C" __declspec(dllexport) const char *NAME = "SBS output";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Duplicate SBS screen into double width buffer and output to glass.";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:

		g_hInstance = hModule;

		CreateThread(NULL, 0, WindowThreadProc, NULL, 0, NULL);

		if (!reshade::register_addon(hModule))
			return FALSE;

		reshade::register_event<reshade::addon_event::init_device>(on_init);
		reshade::register_event<reshade::addon_event::present>(on_present);
		reshade::register_event<reshade::addon_event::destroy_device>(on_destroy);
		reshade::register_event <reshade::addon_event::init_swapchain>(on_init_swapchain);
		
		//reshade::register_overlay(nullptr, draw_settings);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}
