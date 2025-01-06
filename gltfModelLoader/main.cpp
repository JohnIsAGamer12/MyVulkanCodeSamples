// Simple basecode showing how to create a window and attatch a vulkansurface
#define GATEWARE_ENABLE_CORE // All libraries need this
#define GATEWARE_ENABLE_SYSTEM // Graphics libs require system level libraries
#define GATEWARE_ENABLE_GRAPHICS // Enables all Graphics Libraries
// TODO: Part 3a
// Ignore some GRAPHICS libraries we aren't going to use
#define GATEWARE_DISABLE_GDIRECTX11SURFACE // we have another template for this
#define GATEWARE_DISABLE_GDIRECTX12SURFACE // we have another template for this
#define GATEWARE_DISABLE_GRASTERSURFACE // we have another template for this
#define GATEWARE_DISABLE_GOPENGLSURFACE // we have another template for this

#define GATEWARE_ENABLE_MATH
#define GATEWARE_ENABLE_INPUT

// With what we want & what we don't defined we can include the API
#include "gateware-main/Gateware.h"
#include "FileIntoString.h"
#include "renderer.h"
// open some namespaces to compact the code a bit
using namespace GW;
using namespace CORE;
using namespace SYSTEM;
using namespace GRAPHICS;
// lets pop a window and use Vulkan to clear to a red screen
int main()
{
	GWindow win;
	GEventResponder msgs;
	GVulkanSurface vulkan;
	if (+win.Create(0, 0, 1280, 720, GWindowStyle::WINDOWEDBORDERED))
	{
		// win.SetIcon(16, 16, nullptr);
		win.SetWindowName("Jonathan Rivero - Vulkan - gltfModelLoader");
		VkClearValue clrAndDepth[2];
		clrAndDepth[0].color = { {0.75f, 0, 0, 1} };
		clrAndDepth[1].depthStencil = { 1.0f, 0u };
#ifndef NDEBUG
		const char* debugLayers[] = {
			"VK_LAYER_KHRONOS_validation", // standard validation layer
			//"VK_LAYER_RENDERDOC_Capture" // add this if you have installed RenderDoc
		};
		if (+vulkan.Create(win, GW::GRAPHICS::DEPTH_BUFFER_SUPPORT,
			sizeof(debugLayers) / sizeof(debugLayers[0]),
			debugLayers, 0, nullptr, 0, nullptr, false))
#else
		if (+vulkan.Create(win, GW::GRAPHICS::DEPTH_BUFFER_SUPPORT))
#endif
		{
			Renderer renderer(win, vulkan);
			while (+win.ProcessWindowEvents())
			{
				if (+vulkan.StartFrame(2, clrAndDepth))
				{
					std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
					std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();

					std::chrono::duration<float, std::milli> time_span = std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(t2 - t1);

					float dt = time_span.count() / 1000.f;
					renderer.UpdateCamera(dt);

					renderer.Render();
					vulkan.EndFrame(true);
				}
			}
		}
	}
	return 0; // that's all folks
}
