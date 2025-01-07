#include "Dx12Present.h"

#include "reshade.hpp"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <windows.h>


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
