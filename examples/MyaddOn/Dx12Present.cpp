
#include "reshade.hpp"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <windows.h>

#include "Dx12Present.h"

#include "rendercommon.h"

#include <d3dcompiler.h>
#include <strsafe.h>

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3d12.lib")


class CGRSCOMException
{
public:
	CGRSCOMException(HRESULT hr) : m_hrError(hr)
	{
	}
	HRESULT Error() const
	{
		return m_hrError;
	}
private:
	const HRESULT m_hrError;
};

#define THROW_IF_FAILED(hr) {HRESULT _hr = (hr);if (FAILED(_hr)){ throw CGRSCOMException(_hr); }}

bool Dx12Present::init_device(reshade::api::effect_runtime *runtime, reshade::api::resource rtv, reshade::api::resource back_buffer)
{

	bool ret = true;

	// See if we can get a command allocator from reshade
	ID3D12Device *dev = ((ID3D12Device *)d3d12_device->get_native());
	if (!dev) {
		//reshade::log_message(reshade::log_level::info, "Couldn't get a device");
		return false;
	}

	ID3D12CommandAllocator *command_allocator;
	dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocator));
	if (command_allocator == nullptr) {
		//reshade::log_message(reshade::log_level::info, "Couldn't ceate command allocator");
		return false;
	}

	return true;	

}


bool Dx12Present::init_resource(ID3D12Device*	pID3D12Device4,HWND hWnd)
{

	HRESULT hResult;

	DBWQuad.stViewPort = { 0.0f, 0.0f, static_cast<float>(WIDTH_D), static_cast<float>(HEIGHT), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
	DBWQuad.stScissorRect = { 0, 0, static_cast<LONG>(WIDTH_D), static_cast<LONG>(HEIGHT) };

	//
	//创建游戏Swapchain buffer的SRV，我们用它做纹理

	D3D12_DESCRIPTOR_HEAP_DESC stHeapDesc = {};
	stHeapDesc.NumDescriptors = MAX_BACKBUF_COUNT;
	stHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	stHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	//SRV堆
	pID3D12Device4->CreateDescriptorHeap(&stHeapDesc, IID_PPV_ARGS(&DBWQuad.pIDH_DBW_SwapChainHeap));
	//GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDHQuad);

	// 三个SRVs
	D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};
	stSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	stSRVDesc.Format = RTVFormat;
	stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	stSRVDesc.Texture2D.MipLevels = 1;

	D3D12_CPU_DESCRIPTOR_HANDLE stSRVHandle = DBWQuad.pIDH_DBW_SwapChainHeap->GetCPUDescriptorHandleForHeapStart();

	//延迟到present时刻，因为游戏创建的RTV我们并不知道
	/*
	for (UINT i = 0; i < nFrameBackBufCount; i++)
	{
		pID3D12Device4->CreateShaderResourceView(pIARenderTargets[i].Get(), &stSRVDesc, stSRVHandle);
		stSRVHandle.ptr += nSRVDescriptorSize;
	}
	*/

	//
	//创建我们的离屏渲染目标

	// 获取 Direct3D 12 设备和命令队列	
	pID3D12Device4->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&DBWQuad.pICmdAlloc));
	if (DBWQuad.pICmdAlloc == nullptr) {
		reshade::log::message(reshade::log::level::info, "Couldn't ceate command allocator");
		return false;
	}

	pID3D12Device4->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT
				, DBWQuad.pICmdAlloc.Get(), nullptr, IID_PPV_ARGS(&DBWQuad.pICmdList));

	// Describe and create the command queue.	
	D3D12_COMMAND_QUEUE_DESC queue_desc = {};
	queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	pID3D12Device4->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&DBWQuad.pIMainCmdQueue));
	if (DBWQuad.pIMainCmdQueue == nullptr)
	{
		reshade::log::message(reshade::log::level::info, "Couldn't create command queue");
		return false;
	}

	// 创建 DXGI 工厂
	ComPtr<IDXGIFactory4> dxgi_factory;
	CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory));

	//GetDXGIFactoryFromD3D12Device(d3d12_device, &dxgi_factory);


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
	ComPtr<IDXGISwapChain1> swapchain1;
	hResult = dxgi_factory->CreateSwapChainForHwnd(
		DBWQuad.pIMainCmdQueue.Get(), // 必须使用命令队列
		hWnd,
		&swapchain_desc,
		nullptr, // 可选：窗口全屏描述
		nullptr, // 可选：输出限制
		&swapchain1);

	// 升级为 IDXGISwapChain3 接口
	swapchain1.As(&DBWQuad.pISwapChain3);
	
	// 禁用 ALT+ENTER 全屏切换
	dxgi_factory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);


	//得到每个描述符元素的大小
	DBWQuad.nRTVDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	DBWQuad.nSRVDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	DBWQuad.nSampleDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);



	//创建RTV(渲染目标视图)描述符堆
	D3D12_DESCRIPTOR_HEAP_DESC stRTVHeapDesc = {};
	stRTVHeapDesc.NumDescriptors = MAX_BACKBUF_COUNT;
	stRTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	stRTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	pID3D12Device4->CreateDescriptorHeap(&stRTVHeapDesc, IID_PPV_ARGS(&DBWQuad.pIDH_DBWRTV));	

	D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandle = DBWQuad.pIDH_DBWRTV->GetCPUDescriptorHandleForHeapStart();

	for (UINT i = 0; i < MAX_BACKBUF_COUNT; i++)
	{
		DBWQuad.pISwapChain3->GetBuffer(i, IID_PPV_ARGS(&DBWQuad.pIARenderTargets[i]));
		pID3D12Device4->CreateRenderTargetView(DBWQuad.pIARenderTargets[i].Get(), nullptr, stRTVHandle);
		stRTVHandle.ptr += DBWQuad.nRTVDescriptorSize;
	}


	//默认堆上资源默认参数
	D3D12_HEAP_PROPERTIES stDefautHeapProps = {};
	stDefautHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
	stDefautHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	stDefautHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	stDefautHeapProps.CreationNodeMask = 0;
	stDefautHeapProps.VisibleNodeMask = 0;

	D3D12_HEAP_PROPERTIES stUploadHeapProps = {};
	stUploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
	stUploadHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	stUploadHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	stUploadHeapProps.CreationNodeMask = 0;
	stUploadHeapProps.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC stBufferResSesc = {};
	stBufferResSesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	stBufferResSesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	stBufferResSesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	stBufferResSesc.Format = DXGI_FORMAT_UNKNOWN;
	stBufferResSesc.Width = 0;
	stBufferResSesc.Height = 1;
	stBufferResSesc.DepthOrArraySize = 1;
	stBufferResSesc.MipLevels = 1;
	stBufferResSesc.SampleDesc.Count = 1;
	stBufferResSesc.SampleDesc.Quality = 0;


	//
	//创建Depth

	D3D12_CLEAR_VALUE stDepthOptimizedClearValue = {};
	stDepthOptimizedClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	stDepthOptimizedClearValue.DepthStencil.Depth = 1.0f;
	stDepthOptimizedClearValue.DepthStencil.Stencil = 0;

	D3D12_RESOURCE_DESC stDSResDesc = {};
	stDSResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	stDSResDesc.Alignment = 0;
	stDSResDesc.Width = WIDTH_D;
	stDSResDesc.Height = HEIGHT;
	stDSResDesc.DepthOrArraySize = 1;
	stDSResDesc.MipLevels = 0;
	stDSResDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	stDSResDesc.SampleDesc.Count = 1;
	stDSResDesc.SampleDesc.Quality = 0;
	stDSResDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	stDSResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	//使用隐式默认堆创建一个深度蜡板缓冲区，
	//因为基本上深度缓冲区会一直被使用，重用的意义不大，所以直接使用隐式堆，图方便
	THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
		&stDefautHeapProps
		, D3D12_HEAP_FLAG_NONE
		, &stDSResDesc
		, D3D12_RESOURCE_STATE_DEPTH_WRITE
		, &stDepthOptimizedClearValue
		, IID_PPV_ARGS(&DBWQuad.pIDSTex)
	));

	D3D12_DEPTH_STENCIL_VIEW_DESC stDepthStencilDesc = {};
	stDepthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	stDepthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	stDepthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	hResult = pID3D12Device4->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&DBWQuad.pIDH_DBWDSV));

	pID3D12Device4->CreateDepthStencilView(DBWQuad.pIDSTex.Get()
		, &stDepthStencilDesc
		, DBWQuad.pIDH_DBWDSV->GetCPUDescriptorHandleForHeapStart());


	// 创建矩形框的顶点缓冲
	{
		//矩形框的顶点结构变量
		//!!!改成标准尺寸
		ST_VERTEX_QUAD stTriangleVertices[] =
		{
			{ { 0.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f },	{ 0.0f, 0.0f }  },
			{ { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f },	{ 1.0f, 0.0f }  },
			{ { 0.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f },	{ 0.0f, 1.0f }  },
			{ { 1.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f },	{ 1.0f, 1.0f }  }
		};

		UINT nQuadVBSize = sizeof(stTriangleVertices);
		//SIZE_T								szMVPBuf = GRS_UPPER(sizeof(ST_GRS_MVP), 256);
		UINT								nQuadVertexCnt = 0;


		nQuadVertexCnt = _countof(stTriangleVertices);

		stBufferResSesc.Width = nQuadVBSize;
		THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
			&stUploadHeapProps
			, D3D12_HEAP_FLAG_NONE
			, &stBufferResSesc
			, D3D12_RESOURCE_STATE_GENERIC_READ
			, nullptr
			, IID_PPV_ARGS(&DBWQuad.pIVB_DBWQuad)));
		
		UINT8 *pVertexDataBegin = nullptr;
		D3D12_RANGE stReadRange = { 0, 0 };		// We do not intend to read from this resource on the CPU.
		DBWQuad.pIVB_DBWQuad->Map(0, &stReadRange, reinterpret_cast<void **>(&pVertexDataBegin));
		memcpy(pVertexDataBegin, stTriangleVertices, sizeof(stTriangleVertices));
		DBWQuad.pIVB_DBWQuad->Unmap(0, nullptr);

		DBWQuad.stVBView.BufferLocation = DBWQuad.pIVB_DBWQuad->GetGPUVirtualAddress();
		DBWQuad.stVBView.StrideInBytes = sizeof(ST_VERTEX_QUAD);
		DBWQuad.stVBView.SizeInBytes = nQuadVBSize;
	}
	// 创建DBW矩形框渲染用的SRV CBV Sample等
	{
		//MVP矩阵缓冲区
		/*
		stBufferResSesc.Width = szCBMOVBuf;
		GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
			&stUploadHeapProps
			, D3D12_HEAP_FLAG_NONE
			, &stBufferResSesc
			, D3D12_RESOURCE_STATE_GENERIC_READ
			, nullptr
			, IID_PPV_ARGS(&DBWQuad.pICB_DBWMVO)));

		GRS_THROW_IF_FAILED(DBWQuad.pICB_DBWMVO->Map(0, nullptr, reinterpret_cast<void **>(&DBWQuad.pMOV)));
		*/


		//Sampler，采样器
		stHeapDesc.NumDescriptors = 1;  //只有一个Sample
		stHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
		pID3D12Device4->CreateDescriptorHeap(&stHeapDesc, IID_PPV_ARGS(&DBWQuad.pIDH_DBWSampleQuad));

		// Sampler View
		D3D12_SAMPLER_DESC stSamplerDesc = {};
		stSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		stSamplerDesc.MinLOD = 0;
		stSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		stSamplerDesc.MipLODBias = 0.0f;
		stSamplerDesc.MaxAnisotropy = 1;
		stSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		stSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		stSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		stSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

		pID3D12Device4->CreateSampler(&stSamplerDesc, DBWQuad.pIDH_DBWSampleQuad->GetCPUDescriptorHandleForHeapStart());
	}


	//
	//创建Root signature and PSO
	{

		//		
		//创建渲染矩形的根签名对象。 ！！！我们需要CBV吗？如果不用，需要修改成2
		D3D12_DESCRIPTOR_RANGE1 stDSPRanges[3] = {};
		stDSPRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		stDSPRanges[0].NumDescriptors = 1;
		stDSPRanges[0].BaseShaderRegister = 0;
		stDSPRanges[0].RegisterSpace = 0;
		stDSPRanges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;
		stDSPRanges[0].OffsetInDescriptorsFromTableStart = 0;

		stDSPRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		stDSPRanges[1].NumDescriptors = 1;
		stDSPRanges[1].BaseShaderRegister = 0;
		stDSPRanges[1].RegisterSpace = 0;
		stDSPRanges[1].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
		stDSPRanges[1].OffsetInDescriptorsFromTableStart = 0;

		stDSPRanges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
		stDSPRanges[2].NumDescriptors = 1;
		stDSPRanges[2].BaseShaderRegister = 0;
		stDSPRanges[2].RegisterSpace = 0;
		stDSPRanges[2].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
		stDSPRanges[2].OffsetInDescriptorsFromTableStart = 0;

		D3D12_ROOT_PARAMETER1 stRootParameters[3] = {};
		stRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		stRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		stRootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
		stRootParameters[0].DescriptorTable.pDescriptorRanges = &stDSPRanges[0];

		stRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		stRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		stRootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
		stRootParameters[1].DescriptorTable.pDescriptorRanges = &stDSPRanges[1];

		stRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		stRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		stRootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
		stRootParameters[2].DescriptorTable.pDescriptorRanges = &stDSPRanges[2];

		D3D12_VERSIONED_ROOT_SIGNATURE_DESC stRootSignatureDesc = {};
		stRootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
		stRootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		stRootSignatureDesc.Desc_1_1.NumParameters = _countof(stRootParameters);
		stRootSignatureDesc.Desc_1_1.pParameters = stRootParameters;
		stRootSignatureDesc.Desc_1_1.NumStaticSamplers = 0;
		stRootSignatureDesc.Desc_1_1.pStaticSamplers = nullptr;

		ComPtr<ID3DBlob> pISignatureBlob;
		ComPtr<ID3DBlob> pIErrorBlob;

		THROW_IF_FAILED(D3D12SerializeVersionedRootSignature(&stRootSignatureDesc
			, &pISignatureBlob
			, &pIErrorBlob));

		THROW_IF_FAILED(pID3D12Device4->CreateRootSignature(0
			, pISignatureBlob->GetBufferPointer()
			, pISignatureBlob->GetBufferSize()
			, IID_PPV_ARGS(&DBWQuad.pIRSQuad)));

		ComPtr<ID3DBlob> pIDBWBlobVertexShader;
		ComPtr<ID3DBlob> pIDBWBlobPixelShader;

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif
		compileFlags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

		TCHAR pszAppPath[MAX_PATH] = {};
		TCHAR pszShaderFileName[MAX_PATH] = {};
		if (0 == ::GetModuleFileName(nullptr, pszAppPath, MAX_PATH))
		{
			HRESULT_FROM_WIN32(GetLastError());
		}

		WCHAR *lastSlash = _tcsrchr(pszAppPath, _T('\\'));
		if (lastSlash)
		{//删除Exe文件名
			*(lastSlash) = _T('\0');
		}

		StringCchPrintf(pszShaderFileName, MAX_PATH, _T("%s\\SBSExpand.hlsl"), pszAppPath);

		THROW_IF_FAILED(D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
			, "VSMain", "vs_5_0", compileFlags, 0, &pIDBWBlobVertexShader, nullptr));
		THROW_IF_FAILED(D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
			, "PSMain", "ps_5_0", compileFlags, 0, &pIDBWBlobPixelShader, nullptr));


		D3D12_INPUT_ELEMENT_DESC stInputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR",	  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,		 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		// 创建 graphics pipeline state object (PSO)对象
		D3D12_GRAPHICS_PIPELINE_STATE_DESC stPSODesc = {};
		stPSODesc.InputLayout = { stInputElementDescs, _countof(stInputElementDescs) };
		stPSODesc.pRootSignature = DBWQuad.pIRSQuad.Get();
		stPSODesc.VS.BytecodeLength = pIDBWBlobVertexShader->GetBufferSize();
		stPSODesc.VS.pShaderBytecode = pIDBWBlobVertexShader->GetBufferPointer();
		stPSODesc.PS.BytecodeLength = pIDBWBlobPixelShader->GetBufferSize();
		stPSODesc.PS.pShaderBytecode = pIDBWBlobPixelShader->GetBufferPointer();

		stPSODesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		stPSODesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;

		stPSODesc.BlendState.AlphaToCoverageEnable = FALSE;
		stPSODesc.BlendState.IndependentBlendEnable = FALSE;
		stPSODesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		stPSODesc.DepthStencilState.DepthEnable = FALSE;
		stPSODesc.DepthStencilState.StencilEnable = FALSE;
		stPSODesc.SampleMask = UINT_MAX;
		stPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		stPSODesc.NumRenderTargets = 1;
		stPSODesc.RTVFormats[0] = RTVFormat;
		stPSODesc.SampleDesc.Count = 1;

		THROW_IF_FAILED(pID3D12Device4->CreateGraphicsPipelineState(&stPSODesc
			, IID_PPV_ARGS(&DBWQuad.pIPSO_DWBQuad)));
	}
}
