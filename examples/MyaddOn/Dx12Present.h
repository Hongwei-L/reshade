#pragma once

#include "pch.h"

class Dx12Present
{

private:
	reshade::api::device *d3d12_device = nullptr;
	reshade::api::command_list *command_list {};
	reshade::api::resource effect_frame_copy {};
	reshade::api::resource_view game_frame_buffer {};
	reshade::api::format current_buffer_format = reshade::api::format::unknown;

	bool devInitialized;

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
	bool create_swapchain(const reshade::api::resource_desc &effect_resource_desc);
};

