﻿
#include <reshade.hpp>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <windows.h>
#include <wrl.h>			

#include "rendercommon.h"
#include "Dx12Present.h"

#pragma comment(lib, "dxgi.lib")

using namespace Microsoft::WRL;


HINSTANCE g_hInstance;
HWND g_hWnd = NULL;
BOOL g_bExit = FALSE;

const reshade::api::format  rsFormat = reshade::api::format::r8g8b8a8_unorm;

//
// The data across different callback. Reshade provide this to avoid global variable and thread safe
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

DWORD WINAPI WindowThread(LPVOID lpParam)
{
	// 创建窗口
	HWND hwnd = CreateWindowInDLL();

	// 启动消息循环
	MSG msg;
	while (!g_bExit && GetMessage(&msg, hwnd, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}


void on_init(reshade::api::device *device)
{	

	//窗口在线程创建的，等待它创建完
	while (g_hWnd == NULL)
		Sleep(10);

	//创建私有数据
	device->create_private_data<SBSRenderData>();
	SBSRenderData &devData = device->get_private_data <SBSRenderData>();


	ID3D12Device *d3d12_device = ((ID3D12Device *)device->get_native());

	devData.dx12Present.init_resource(d3d12_device,g_hWnd);
}

static void on_present(reshade::api::command_queue *queue, reshade::api::swapchain *swapChain, const reshade::api::rect *, const reshade::api::rect *, uint32_t, const reshade::api::rect *)
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

		devData.gameSRVCreated = true;
	}


	reshade::api::command_list *cmd_list = queue->get_immediate_command_list();
	const reshade::api::resource back_buffer = swapChain->get_current_back_buffer();

	cmd_list->barrier(back_buffer, reshade::api::resource_usage::present, reshade::api::resource_usage::shader_resource_pixel| reshade::api::resource_usage::shader_resource);
	
	queue->flush_immediate_command_list();

	//queue->wait_idle();

	devData.dx12Present.on_present(swapChain->get_current_back_buffer_index());

	cmd_list->barrier(back_buffer, reshade::api::resource_usage::shader_resource_pixel | reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::present);

}


static void on_destroy(reshade::api::device *device)
{
	SBSRenderData &devData = device->get_private_data <SBSRenderData>();
	devData.dx12Present.uninit_resource();

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

HANDLE hThread = 0;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:

		g_hInstance = hModule;

		hThread = CreateThread(NULL, 0, WindowThread, NULL, 0, NULL);

		if (!reshade::register_addon(hModule))
			return FALSE;

		reshade::register_event<reshade::addon_event::init_device>(on_init);
		reshade::register_event<reshade::addon_event::present>(on_present);
		reshade::register_event<reshade::addon_event::destroy_device>(on_destroy);
		reshade::register_event <reshade::addon_event::init_swapchain>(on_init_swapchain);		
		
		break;

	case DLL_PROCESS_DETACH:
		g_bExit = TRUE;

		//TerminateThread(hThread,-1);
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}


/*

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
*/

