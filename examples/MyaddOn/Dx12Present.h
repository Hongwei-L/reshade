#pragma once

#include <SDKDDKVer.h>
#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // �� Windows ͷ���ų�����ʹ�õ�����
#include <windows.h>

#include <d3dcommon.h>
//#include <D3dx12.h>

#include "RenderCommon.h"

#include <tchar.h>
#include <wrl.h>			//���WTL֧�� ����ʹ��COM
#include <dxgi1_6.h>
#include <d3d12.h>			//for d3d12

#include <DirectXMath.h>

using namespace std;
using namespace Microsoft;
using namespace Microsoft::WRL;
using namespace DirectX;

// ����ͶӰ�ĳ�������
struct ST_GRS_CB_MVO
{
	//XMFLOAT4X4 m_mMVO;	   //Model * View * Orthographic 
};

// ���ο�Ķ���ṹ
struct ST_VERTEX_QUAD
{
	XMFLOAT4 m_v4Position;		//Position
	XMFLOAT4 m_vClr;		//Color
	XMFLOAT2 m_vTxc;		//Texcoord
};


struct dbwQuad {

	//
	//������Ҫ����Դ��
	// 1) RTV��������, �������������ڶ����RTV��Դ��swapchain�����ˣ�
	// 2) Depth�������ѣ�����������depth��Դָ��
	// 3����Ϸpresent RTV��Ӧ��SRV����Ϊ���ǰ���ϷRTV��Ϊ����������Ҫ������Ӧ��SRV
	// 4��Sampler���󣬶������SRV���в���
	// 5��PSO����
	// 6�����㻺�壬��������CBV

	ComPtr<ID3D12RootSignature>			pIRSQuad;
	ComPtr<ID3D12PipelineState>			pIPSOQuad;
	ComPtr<ID3D12CommandQueue>			pIMainCmdQueue;
	ComPtr<ID3D12CommandAllocator>		pICmdAlloc;
	ComPtr<ID3D12GraphicsCommandList>	pICmdList;


	ComPtr<ID3D12DescriptorHeap>		pIDH_DBWQuad;	//RTV��������
	ComPtr<ID3D12DescriptorHeap>		pIDH_DBWSampleQuad;	//sampler

	ComPtr<ID3D12Resource>				pIVB_DBWQuad;	//QUAD���㻺��
	ComPtr<ID3D12Resource>			    pICB_DBWMVO;	//��������

	//ComPtr<ID3D12Resource>				pIRes_DBWRenderTarget;  //2x width Render Target
	ComPtr<ID3D12Resource>				pIARenderTargets[MAX_BACKBUF_COUNT];
	ComPtr<IDXGISwapChain3>				pISwapChain3;

	ComPtr<ID3D12Resource>				pIDSTex;		//depth buffer
	ComPtr<ID3D12DescriptorHeap>		pIDH_DBWRTV;	//RTV heap
	ComPtr<ID3D12DescriptorHeap>		pIDH_DBWDSV;	//Depth view, DSV heap

	ST_GRS_CB_MVO *pMOV = nullptr;		//MVP�����ӳ��CPU��ַ������ѭ���п���ֱ�����︴��

	//������Ҫ��SWAPCHAIN��BufferҲҪ��Ϊ����ʹ�ã���Ҫ��������
	ComPtr<ID3D12DescriptorHeap>				pIDH_DBW_SwapChainHeap;

	ComPtr<ID3D12PipelineState>			pIPSO_DWBQuad;	//PSO	

	D3D12_VIEWPORT						stViewPort;// = { 0.0f, 0.0f, static_cast<float>(DBW_width), static_cast<float>(DBW_Height), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
	D3D12_RECT							stScissorRect;// = { 0, 0, static_cast<LONG>(DBW_width), static_cast<LONG>(DBW_Height) };

	//����
	D3D12_VERTEX_BUFFER_VIEW			stVBView = {};


	int	nRTVDescriptorSize;
	int nSRVDescriptorSize;
	int nSampleDescriptorSize;

};




class Dx12Present
{

private:
	reshade::api::device *d3d12_device = nullptr;
	reshade::api::command_list *command_list {};
	reshade::api::resource effect_frame_copy {};
	reshade::api::resource_view game_frame_buffer {};
	reshade::api::format current_buffer_format = reshade::api::format::unknown;

	bool devInitialized;

	struct dbwQuad DBWQuad;

public:

	Dx12Present() = default;

	/// \brief Initialized the SR weaver appropriate for the graphics API
		/// \param runtime Represents the reshade effect runtime
		/// \param rtv Represents the buffer that the weaver uses as a source to weave with
		/// \param back_buffer Represents the current back buffer from ReShade
		/// \return A bool representing if the weaver was initialized successfully
	bool init_device(reshade::api::effect_runtime *runtime, reshade::api::resource rtv, reshade::api::resource back_buffer);

	/// \brief Creates and reset the effect copy resource so it is similar to the back buffer resource, then use it as weaver input.
	/// \param effect_resource_desc ReShade resource representing the currently selected back buffer description
	/// \return A bool respresenting if the effect frame copy was successful
	//bool create_swapchain(const reshade::api::resource_desc &effect_resource_desc);

	bool init_resource(ID3D12Device*				pID3D12Device4, HWND hWnd);

};

