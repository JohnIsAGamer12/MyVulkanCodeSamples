// minimalistic code to draw a single triangle, this is not part of the API.
#include "shaderc/shaderc.h" // needed for compiling shaders at runtime
#ifdef _WIN32 // must use MT platform DLL libraries on windows
#pragma comment(lib, "shaderc_combined.lib") 
#endif

// Includes
#include <chrono>

#define NUMBEROFGRIDVERTS 625

void PrintLabeledDebugString(const char* label, const char* toPrint)
{
	std::cout << label << toPrint << std::endl;

	//OutputDebugStringA is a windows-only function 
#if defined WIN32 
	OutputDebugStringA(label);
	OutputDebugStringA(toPrint);
#endif
}

class Renderer
{
	// proxy handles
	GW::SYSTEM::GWindow win;
	GW::GRAPHICS::GVulkanSurface vlk;
	VkRenderPass renderPass;
	GW::CORE::GEventReceiver shutdown;

	// what we need at a minimum to draw a triangle
	VkDevice device = nullptr;
	VkPhysicalDevice physicalDevice = nullptr;
	VkBuffer vertexHandle = nullptr;
	VkDeviceMemory vertexData = nullptr;
	VkShaderModule vertexShader = nullptr;
	VkShaderModule fragmentShader = nullptr;
	VkPipeline pipeline = nullptr;
	VkPipelineLayout pipelineLayout = nullptr;

	unsigned int windowWidth, windowHeight;

	struct Vertex { float x, y, z, w; };

	GW::MATH::GMatrix matrixMath;

	GW::MATH::GMATRIXF grids[6];

	struct SHADER_VARS
	{
		GW::MATH::GMATRIXF worldMatrix[6];
		GW::MATH::GMATRIXF viewMatrix;
		GW::MATH::GMATRIXF projectionMatrix;
	} shaderVars;

	std::vector<VkBuffer> uniformHandle;
	std::vector<VkDeviceMemory> uniformData;
	unsigned int maxFrames;

	VkDescriptorSetLayout descriptor_set_layout;

	VkDescriptorPool _descriptorPool;

	std::vector<VkDescriptorSet> descriptorSets;

	GW::MATH::GMATRIXF viewMatrix = GW::MATH::GIdentityMatrixF;

	GW::MATH::GMATRIXF projectionMatrix = GW::MATH::GIdentityMatrixF;

	GW::INPUT::GInput inputProxy;
	GW::INPUT::GController controllerProxy;

public:
	Renderer(GW::SYSTEM::GWindow _win, GW::GRAPHICS::GVulkanSurface _vlk)
	{
		win = _win;
		vlk = _vlk;


		matrixMath.Create();

		CreateCubeGridMatrix();

		CreateViewMatrix();

		CreateProjectionMatrix();

		inputProxy.Create(win);
		controllerProxy.Create();

		UpdateWindowDimensions();
		InitializeGraphics();
		BindShutdownCallback();
	}

	void CreateProjectionMatrix()
	{
		float aspect;
		vlk.GetAspectRatio(aspect);

		matrixMath.ProjectionVulkanLHF((65 * 3.14f) / 180.f, aspect, 0.1f, 100.f, projectionMatrix);
		shaderVars.projectionMatrix = projectionMatrix;
	}

	void CreateViewMatrix()
	{
		viewMatrix = GW::MATH::GIdentityMatrixF;
		matrixMath.LookAtLHF(GW::MATH::GVECTORF{ 0.25f, -0.125f, -0.25f, 1.f }, GW::MATH::GVECTORF{ 0, 0, 0 }, GW::MATH::GVECTORF{0, 1, 0}, viewMatrix);
	}

	void CreateCubeGridMatrix()
	{
		for (size_t i = 0; i < 6; i++)
		{
			grids[i] = GW::MATH::GIdentityMatrixF;

			switch (i)
			{
			case 0:
			{
				matrixMath.TranslateLocalF(grids[i], GW::MATH::GVECTORF{ 0, -0.5f, 0, 1 }, grids[i]);
				matrixMath.RotateXLocalF(grids[i], 1.57f, grids[i]);
				break;
			}
			case 1:
			{
				matrixMath.TranslateLocalF(grids[i], GW::MATH::GVECTORF{ 0, 0.75f, 0, 1 }, grids[i]);
				matrixMath.RotateXLocalF(grids[i], 1.57f, grids[i]);
				break;
			}
			case 2:
			{
				matrixMath.TranslateLocalF(grids[i], GW::MATH::GVECTORF{ 0, 0.1f, -0.6f, 1 }, grids[i]);
				break;
			}
			case 3:
			{
				matrixMath.TranslateLocalF(grids[i], GW::MATH::GVECTORF{ 0, 0.1f, 0.65f, 1 }, grids[i]);
				break;
			}
			case 4:
			{
				matrixMath.TranslateLocalF(grids[i], GW::MATH::GVECTORF{ -0.65f, 0.15f, 0, 1 }, grids[i]);
				matrixMath.RotateXLocalF(grids[i], 1.57f, grids[i]);
				matrixMath.RotateYLocalF(grids[i], 1.57f, grids[i]);
				break;
			}
			case 5:
			{
				matrixMath.TranslateLocalF(grids[i], GW::MATH::GVECTORF{ 0.6f, 0.15f, 0, 1 }, grids[i]);
				matrixMath.RotateXLocalF(grids[i], 1.57f, grids[i]);
				matrixMath.RotateYLocalF(grids[i], 1.57f, grids[i]);
				break;
			}
			default:
				break;
			}

			shaderVars.worldMatrix[i] = grids[i];
		}
	}

private:
	void UpdateWindowDimensions()
	{
		win.GetClientWidth(windowWidth);
		win.GetClientHeight(windowHeight);
	}

	void InitializeGraphics()
	{
		GetHandlesFromSurface();
		InitializeVertexBuffer();

		vlk.GetSwapchainImageCount(maxFrames);

		uniformHandle.resize(maxFrames);
		uniformData.resize(maxFrames);
		descriptorSets.resize(maxFrames);

		for (int i = 0; i < maxFrames; i++)
		{
			GvkHelper::create_buffer(physicalDevice, device, sizeof(SHADER_VARS), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				&uniformHandle[i], &uniformData[i]);
			GvkHelper::write_to_buffer(device, uniformData[i], &shaderVars, sizeof(SHADER_VARS));
		}

		VkDescriptorPoolSize descriptor_poolsize = {};
		descriptor_poolsize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptor_poolsize.descriptorCount = static_cast<uint32_t>(maxFrames);

		VkDescriptorPoolCreateInfo descriptor_pool_createinfo = {};
		descriptor_pool_createinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptor_pool_createinfo.maxSets = static_cast<uint32_t>(maxFrames);
		descriptor_pool_createinfo.poolSizeCount = 1;
		descriptor_pool_createinfo.pPoolSizes = &descriptor_poolsize;

		vkCreateDescriptorPool(device, &descriptor_pool_createinfo, nullptr, &_descriptorPool);

		VkDescriptorSetLayoutBinding descriptor_set_layout_binding = {};
		descriptor_set_layout_binding.binding = 0;
		descriptor_set_layout_binding.descriptorCount = 1;
		descriptor_set_layout_binding.pImmutableSamplers = nullptr;
		descriptor_set_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptor_set_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info = {};
		descriptor_set_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptor_set_layout_info.bindingCount = 1;
		descriptor_set_layout_info.flags = 0;
		descriptor_set_layout_info.pNext = nullptr;
		descriptor_set_layout_info.pBindings = &descriptor_set_layout_binding;

		vkCreateDescriptorSetLayout(device, &descriptor_set_layout_info, nullptr, &descriptor_set_layout);

		VkDescriptorSetAllocateInfo descriptor_set_allocateinfo = {};
		descriptor_set_allocateinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptor_set_allocateinfo.pSetLayouts = &descriptor_set_layout;
		descriptor_set_allocateinfo.descriptorSetCount = 1;
		descriptor_set_allocateinfo.descriptorPool = _descriptorPool;

		VkDescriptorBufferInfo descriptor_buffer_info[2] = {};
		for (int i = 0; i < maxFrames; i++)
		{
			vkAllocateDescriptorSets(device, &descriptor_set_allocateinfo, &descriptorSets[i]);

			descriptor_buffer_info[i].buffer = uniformHandle[i];
			descriptor_buffer_info[i].range = sizeof(SHADER_VARS);

			VkWriteDescriptorSet write_descriptorset = {};
			write_descriptorset.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write_descriptorset.descriptorCount = 1;
			write_descriptorset.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			write_descriptorset.dstSet = descriptorSets[i];
			write_descriptorset.pBufferInfo = &descriptor_buffer_info[i];

			vkUpdateDescriptorSets(device, 1, &write_descriptorset, 0, nullptr);
		}


		CompileShaders();
		InitializeGraphicsPipeline();
	}

	void GetHandlesFromSurface()
	{
		vlk.GetDevice((void**)&device);
		vlk.GetPhysicalDevice((void**)&physicalDevice);
		vlk.GetRenderPass((void**)&renderPass);
	}

	void InitializeVertexBuffer()
	{
		Vertex Grid[] =
		{
			// Vertical
			{ -0.65f,  0.65f, 0, 1 },
			{ -0.65f, -0.6f, 0, 1 },
			{ -0.6f,  0.65f, 0, 1 },
			{ -0.6f, -0.6f, 0, 1 },
			{ -0.55f,  0.65f, 0, 1 },
			{ -0.55f, -0.6f, 0, 1 },
			{ -0.5f,  0.65f, 0, 1 },
			{ -0.5f, -0.6f, 0, 1 },
			{ -0.45f,  0.65f, 0, 1 },
			{ -0.45f, -0.6f, 0, 1 },
			{ -0.4f,  0.65f, 0, 1 },
			{ -0.4f, -0.6f, 0, 1 },
			{ -0.35f,  0.65f, 0, 1 },
			{ -0.35f, -0.6f, 0, 1 },
			{ -0.3f,  0.65f, 0, 1 },
			{ -0.3f, -0.6f, 0, 1 },
			{ -0.25f,  0.65f, 0, 1 },
			{ -0.25f, -0.6f, 0, 1 },
			{ -0.2f,  0.65f, 0, 1 },
			{ -0.2f, -0.6f, 0, 1 },
			{ -0.15f,  0.65f, 0, 1 },
			{ -0.15f, -0.6f, 0, 1 },
			{ -0.1f,  0.65f, 0, 1 },
			{ -0.1f, -0.6f, 0, 1 },
			{ -0.05f,  0.65f, 0, 1 },
			{ -0.05f, -0.6f, 0, 1 },
			{    0,   0.65f, 0, 1 },
			{    0,  -0.6f, 0, 1 },
			{  0.05f,  0.65f, 0, 1 },
			{  0.05f, -0.6f, 0, 1 },
			{  0.1f,  0.65f, 0, 1 },
			{  0.1f, -0.6f, 0, 1 },
			{  0.15f,  0.65f, 0, 1 },
			{  0.15f, -0.6f, 0, 1 },
			{  0.2f,  0.65f, 0, 1 },
			{  0.2f, -0.6f, 0, 1 },
			{  0.25f,  0.65f, 0, 1 },
			{  0.25f, -0.6f, 0, 1 },
			{  0.3f,  0.65f, 0, 1 },
			{  0.3f, -0.6f, 0, 1 },
			{  0.35f,  0.65f, 0, 1 },
			{  0.35f, -0.6f, 0, 1 },
			{  0.4f,  0.65f, 0, 1 },
			{  0.4f, -0.6f, 0, 1 },
			{  0.45f,  0.65f, 0, 1 },
			{  0.45f, -0.6f, 0, 1 },
			{  0.5f,  0.65f, 0, 1 },
			{  0.5f, -0.6f, 0, 1 },
			{  0.55f,  0.65f, 0, 1 },
			{  0.55f, -0.6f, 0, 1 },
			{  0.6f,  0.65f, 0, 1 },
			{  0.6f, -0.6f, 0, 1 },
			// Cross
			{ -0.65f, 0.65f, 0, 1},
			{ 0.6f, 0.65f, 0, 1},
			{ -0.65f, 0.6f, 0, 1},
			{ 0.6f, 0.6f, 0, 1},
			{ -0.65f, 0.55f, 0, 1},
			{ 0.6f, 0.55f, 0, 1},
			{ -0.65f, 0.5f, 0, 1},
			{ 0.6f, 0.5f, 0, 1},
			{ -0.65f, 0.45f, 0, 1},
			{ 0.6f, 0.45f, 0, 1},
			{ -0.65f, 0.4f, 0, 1},
			{ 0.6f, 0.4f, 0, 1},
			{ -0.65f, 0.35f, 0, 1},
			{ 0.6f, 0.35f, 0, 1},
			{ -0.65f, 0.3f, 0, 1},
			{ 0.6f, 0.3f, 0, 1},
			{ -0.65f, 0.25f, 0, 1},
			{ 0.6f, 0.25f, 0, 1},
			{ -0.65f, 0.2f, 0, 1},
			{ 0.6f, 0.2f, 0, 1},
			{ -0.65f, 0.15f, 0, 1},
			{ 0.6f, 0.15f, 0, 1},
			{ -0.65f, 0.1f, 0, 1},
			{ 0.6f, 0.1f, 0, 1},
			{ -0.65f, 0.05f, 0, 1},
			{ 0.6f, 0.05f, 0, 1},
			{ -0.65f, 0, 0, 1},
			{ 0.6f, 0, 0, 1},
			{ -0.65f, -0.05f, 0, 1},
			{ 0.6f, -0.05f, 0, 1},
			{ -0.65f, -0.1f, 0, 1},
			{ 0.6f, -0.1f, 0, 1},
			{ -0.65f, -0.15f, 0, 1},
			{ 0.6f, -0.15f, 0, 1},
			{ -0.65f, -0.2f, 0, 1},
			{ 0.6f, -0.2f, 0, 1},
			{ -0.65f, -0.25f, 0, 1},
			{ 0.6f, -0.25f, 0, 1},
			{ -0.65f, -0.3f, 0, 1},
			{ 0.6f, -0.3f, 0, 1},
			{ -0.65f, -0.35f, 0, 1},
			{ 0.6f, -0.35f, 0, 1},
			{ -0.65f, -0.4f, 0, 1},
			{ 0.6f, -0.4f, 0, 1},
			{ -0.65f, -0.45f, 0, 1},
			{ 0.6f, -0.45f, 0, 1},
			{ -0.65f, -0.5f, 0, 1},
			{ 0.6f, -0.5f, 0, 1 },
			{ -0.65f, -0.55f, 0, 1},
			{ 0.6f, -0.55f, 0, 1},
			{ -0.65f, -0.6f, 0, 1},
			{ 0.6f, -0.6f, 0, 1}
		};

		CreateVertexBuffer(&Grid[0], sizeof(Grid));
	}

	void CreateVertexBuffer(const void* data, unsigned int sizeInBytes)
	{
		GvkHelper::create_buffer(physicalDevice, device, sizeInBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&vertexHandle, &vertexData);
		GvkHelper::write_to_buffer(device, vertexData, data, sizeInBytes); // Transfer line data to the vertex buffer. 
	}

	void CompileShaders()
	{
		// Intialize runtime shader compiler HLSL -> SPIRV
		shaderc_compiler_t compiler = shaderc_compiler_initialize();
		shaderc_compile_options_t options = CreateCompileOptions();

		CompileVertexShader(compiler, options);
		CompileFragmentShader(compiler, options);

		// Free runtime shader compiler resources
		shaderc_compile_options_release(options);
		shaderc_compiler_release(compiler);
	}

	shaderc_compile_options_t CreateCompileOptions()
	{
		shaderc_compile_options_t retval = shaderc_compile_options_initialize();
		shaderc_compile_options_set_source_language(retval, shaderc_source_language_hlsl);
		shaderc_compile_options_set_invert_y(retval, false);	// TODO: Part 3e
#ifndef NDEBUG
		shaderc_compile_options_set_generate_debug_info(retval);
#endif
		return retval;
	}

	void CompileVertexShader(const shaderc_compiler_t& compiler, const shaderc_compile_options_t& options)
	{
		std::string vertexShaderSource = ReadFileIntoString("../../uniform_buffers/VertexShader.hlsl");

		shaderc_compilation_result_t result = shaderc_compile_into_spv( // compile
			compiler, vertexShaderSource.c_str(), vertexShaderSource.length(),
			shaderc_vertex_shader, "main.vert", "main", options);

		if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) // errors?
		{
			PrintLabeledDebugString("Vertex Shader Errors: \n", shaderc_result_get_error_message(result));
			abort(); //Vertex shader failed to compile! 
			return;
		}

		GvkHelper::create_shader_module(device, shaderc_result_get_length(result), // load into Vulkan
			(char*)shaderc_result_get_bytes(result), &vertexShader);

		shaderc_result_release(result); // done
	}

	void CompileFragmentShader(const shaderc_compiler_t& compiler, const shaderc_compile_options_t& options)
	{
		std::string fragmentShaderSource = ReadFileIntoString("../../uniform_buffers/FragmentShader.hlsl");

		shaderc_compilation_result_t result = shaderc_compile_into_spv( // compile
			compiler, fragmentShaderSource.c_str(), fragmentShaderSource.length(),
			shaderc_fragment_shader, "main.frag", "main", options);

		if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) // errors?
		{
			PrintLabeledDebugString("Fragment Shader Errors: \n", shaderc_result_get_error_message(result));
			abort(); //Fragment shader failed to compile! 
			return;
		}

		GvkHelper::create_shader_module(device, shaderc_result_get_length(result), // load into Vulkan
			(char*)shaderc_result_get_bytes(result), &fragmentShader);

		shaderc_result_release(result); // done
	}

	// Create Pipeline & Layout (Thanks Tiny!)
	void InitializeGraphicsPipeline()
	{
		VkPipelineShaderStageCreateInfo stage_create_info[2] = {};

		// Create Stage Info for Vertex Shader
		stage_create_info[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage_create_info[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		stage_create_info[0].module = vertexShader;
		stage_create_info[0].pName = "main";

		// Create Stage Info for Fragment Shader
		stage_create_info[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage_create_info[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stage_create_info[1].module = fragmentShader;
		stage_create_info[1].pName = "main";


		VkPipelineInputAssemblyStateCreateInfo assembly_create_info = CreateVkPipelineInputAssemblyStateCreateInfo();
		VkVertexInputBindingDescription vertex_binding_description = CreateVkVertexInputBindingDescription();

		VkVertexInputAttributeDescription vertex_attribute_descriptions[1];
		vertex_attribute_descriptions[0].binding = 0;
		vertex_attribute_descriptions[0].location = 0;
		vertex_attribute_descriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		vertex_attribute_descriptions[0].offset = 0;

		VkPipelineVertexInputStateCreateInfo input_vertex_info = CreateVkPipelineVertexInputStateCreateInfo(&vertex_binding_description, 1, vertex_attribute_descriptions, 1);
		VkViewport viewport = CreateViewportFromWindowDimensions();
		VkRect2D scissor = CreateScissorFromWindowDimensions();
		VkPipelineViewportStateCreateInfo viewport_create_info = CreateVkPipelineViewportStateCreateInfo(&viewport, 1, &scissor, 1);
		VkPipelineRasterizationStateCreateInfo rasterization_create_info = CreateVkPipelineRasterizationStateCreateInfo();
		VkPipelineMultisampleStateCreateInfo multisample_create_info = CreateVkPipelineMultisampleStateCreateInfo();
		VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info = CreateVkPipelineDepthStencilStateCreateInfo();
		VkPipelineColorBlendAttachmentState color_blend_attachment_state = CreateVkPipelineColorBlendAttachmentState();
		VkPipelineColorBlendStateCreateInfo color_blend_create_info = CreateVkPipelineColorBlendStateCreateInfo(&color_blend_attachment_state, 1);

		VkDynamicState dynamic_states[2] =
		{
			// By setting these we do not need to re-create the pipeline on Resize
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};

		VkPipelineDynamicStateCreateInfo dynamic_create_info = CreateVkPipelineDynamicStateCreateInfo(dynamic_states, 2);

		CreatePipelineLayout();

		// Pipeline State... (FINALLY) 
		VkGraphicsPipelineCreateInfo pipeline_create_info = {};
		pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline_create_info.stageCount = 2;
		pipeline_create_info.pStages = stage_create_info;
		pipeline_create_info.pInputAssemblyState = &assembly_create_info;
		pipeline_create_info.pVertexInputState = &input_vertex_info;
		pipeline_create_info.pViewportState = &viewport_create_info;
		pipeline_create_info.pRasterizationState = &rasterization_create_info;
		pipeline_create_info.pMultisampleState = &multisample_create_info;
		pipeline_create_info.pDepthStencilState = &depth_stencil_create_info;
		pipeline_create_info.pColorBlendState = &color_blend_create_info;
		pipeline_create_info.pDynamicState = &dynamic_create_info;
		pipeline_create_info.layout = pipelineLayout;
		pipeline_create_info.renderPass = renderPass;
		pipeline_create_info.subpass = 0;
		pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;

		vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &pipeline);
	}

	VkPipelineShaderStageCreateInfo CreateVertexShaderStageCreateInfo()
	{
		VkPipelineShaderStageCreateInfo retval;
		retval.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		retval.stage = VK_SHADER_STAGE_VERTEX_BIT;
		retval.module = vertexShader;
		retval.pName = "main";
		return retval;
	}

	VkPipelineInputAssemblyStateCreateInfo CreateVkPipelineInputAssemblyStateCreateInfo()
	{
		VkPipelineInputAssemblyStateCreateInfo retval = {};
		retval.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		retval.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
		retval.primitiveRestartEnable = false;
		return retval;
	}

	VkVertexInputBindingDescription CreateVkVertexInputBindingDescription()
	{
		VkVertexInputBindingDescription retval = {};
		retval.binding = 0;
		retval.stride = sizeof(Vertex);
		retval.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return retval;
	}

	VkPipelineVertexInputStateCreateInfo CreateVkPipelineVertexInputStateCreateInfo(
		VkVertexInputBindingDescription* inputBindingDescriptions, unsigned int bindingCount,
		VkVertexInputAttributeDescription* vertexAttributeDescriptions, unsigned int attributeCount)
	{
		VkPipelineVertexInputStateCreateInfo retval = {};
		retval.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		retval.vertexBindingDescriptionCount = bindingCount;
		retval.pVertexBindingDescriptions = inputBindingDescriptions;
		retval.vertexAttributeDescriptionCount = attributeCount;
		retval.pVertexAttributeDescriptions = vertexAttributeDescriptions;
		return retval;
	}

	VkViewport CreateViewportFromWindowDimensions()
	{
		VkViewport retval = {};
		retval.x = 0;
		retval.y = 0;
		retval.width = static_cast<float>(windowWidth);
		retval.height = static_cast<float>(windowHeight);
		retval.minDepth = 0;
		retval.maxDepth = 1;
		return retval;
	}

	VkRect2D CreateScissorFromWindowDimensions()
	{
		VkRect2D retval = {};
		retval.offset.x = 0;
		retval.offset.y = 0;
		retval.extent.width = windowWidth;
		retval.extent.height = windowHeight;
		return retval;
	}

	VkPipelineViewportStateCreateInfo CreateVkPipelineViewportStateCreateInfo(VkViewport* viewports, unsigned int viewportCount, VkRect2D* scissors, unsigned int scissorCount)
	{
		VkPipelineViewportStateCreateInfo retval = {};
		retval.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		retval.viewportCount = viewportCount;
		retval.pViewports = viewports;
		retval.scissorCount = scissorCount;
		retval.pScissors = scissors;
		return retval;
	}

	VkPipelineRasterizationStateCreateInfo CreateVkPipelineRasterizationStateCreateInfo()
	{
		VkPipelineRasterizationStateCreateInfo retval = {};
		retval.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		retval.rasterizerDiscardEnable = VK_FALSE;
		retval.polygonMode = VK_POLYGON_MODE_FILL;
		retval.lineWidth = 1.0f;
		retval.cullMode = VK_CULL_MODE_BACK_BIT;
		retval.frontFace = VK_FRONT_FACE_CLOCKWISE;
		retval.depthClampEnable = VK_FALSE;
		retval.depthBiasEnable = VK_FALSE;
		retval.depthBiasClamp = 0.0f;
		retval.depthBiasConstantFactor = 0.0f;
		retval.depthBiasSlopeFactor = 0.0f;
		return retval;
	}

	VkPipelineMultisampleStateCreateInfo CreateVkPipelineMultisampleStateCreateInfo()
	{
		VkPipelineMultisampleStateCreateInfo retval = {};
		retval.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		retval.sampleShadingEnable = VK_FALSE;
		retval.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		retval.minSampleShading = 1.0f;
		retval.pSampleMask = VK_NULL_HANDLE;
		retval.alphaToCoverageEnable = VK_FALSE;
		retval.alphaToOneEnable = VK_FALSE;
		return retval;
	}

	VkPipelineDepthStencilStateCreateInfo CreateVkPipelineDepthStencilStateCreateInfo()
	{
		VkPipelineDepthStencilStateCreateInfo retval = {};
		retval.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		retval.depthTestEnable = VK_TRUE;
		retval.depthWriteEnable = VK_TRUE;
		retval.depthCompareOp = VK_COMPARE_OP_LESS;
		retval.depthBoundsTestEnable = VK_FALSE;
		retval.minDepthBounds = 0.0f;
		retval.maxDepthBounds = 1.0f;
		retval.stencilTestEnable = VK_FALSE;
		return retval;
	}

	VkPipelineColorBlendAttachmentState CreateVkPipelineColorBlendAttachmentState()
	{
		VkPipelineColorBlendAttachmentState retval = {};
		retval.colorWriteMask = 0xF;
		retval.blendEnable = VK_FALSE;
		retval.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
		retval.dstColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
		retval.colorBlendOp = VK_BLEND_OP_ADD;
		retval.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		retval.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
		retval.alphaBlendOp = VK_BLEND_OP_ADD;
		return retval;
	}

	VkPipelineColorBlendStateCreateInfo CreateVkPipelineColorBlendStateCreateInfo(VkPipelineColorBlendAttachmentState* attachments, unsigned int attachmentCount)
	{
		VkPipelineColorBlendStateCreateInfo retval = {};
		retval.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		retval.logicOpEnable = VK_FALSE;
		retval.logicOp = VK_LOGIC_OP_COPY;
		retval.attachmentCount = attachmentCount;
		retval.pAttachments = attachments;
		retval.blendConstants[0] = 0.0f;
		retval.blendConstants[1] = 0.0f;
		retval.blendConstants[2] = 0.0f;
		retval.blendConstants[3] = 0.0f;
		return retval;
	}

	VkPipelineDynamicStateCreateInfo CreateVkPipelineDynamicStateCreateInfo(VkDynamicState* dynamicStates, unsigned int dynamicStateCount)
	{
		VkPipelineDynamicStateCreateInfo retval = {};
		retval.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		retval.dynamicStateCount = dynamicStateCount;
		retval.pDynamicStates = dynamicStates;
		return retval;
	}

	void CreatePipelineLayout()
	{
		VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};
		pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipeline_layout_create_info.setLayoutCount = 1;
		pipeline_layout_create_info.pSetLayouts = &descriptor_set_layout;
		pipeline_layout_create_info.pushConstantRangeCount = 0;
		pipeline_layout_create_info.pPushConstantRanges = nullptr;

		vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &pipelineLayout);
	}

	void BindShutdownCallback()
	{
		// GVulkanSurface will inform us when to release any allocated resources
		shutdown.Create(vlk, [&]() {
			if (+shutdown.Find(GW::GRAPHICS::GVulkanSurface::Events::RELEASE_RESOURCES, true)) {
				CleanUp(); // unlike D3D we must be careful about destroy timing
			}
			});
	}


public:
	void Render()
	{
		VkCommandBuffer commandBuffer = GetCurrentCommandBuffer();

		for (int i = 0; i < maxFrames; i++)
		{
			GvkHelper::write_to_buffer(device, uniformData[i], &shaderVars, sizeof(SHADER_VARS));
		}

		SetUpPipeline(commandBuffer);

		shaderVars.worldMatrix[0] = grids[0];
		for (size_t i = 0; i < static_cast<uint32_t>(maxFrames); i++)
		{
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[i], 0, nullptr);
		}

		vkCmdDraw(commandBuffer, NUMBEROFGRIDVERTS, 6, 0, 0);
	}

	void UpdateCamera()
	{
		std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
		std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
		std::chrono::duration<float> time_span = std::chrono::duration_cast<std::chrono::duration<float>>(t2 - t1);

		GW::MATH::GMATRIXF cameraMatrix = GW::MATH::GIdentityMatrixF;
		matrixMath.InverseF(viewMatrix, cameraMatrix);

		float spaceInput = 0;
		float lShiftInput = 0;
		float rTriggerInput = 0;
		float lTriggerInput = 0;
		const float Camera_Speed = 300.f;

		inputProxy.GetState(G_KEY_SPACE, spaceInput);              
		inputProxy.GetState(G_KEY_LEFTSHIFT, lShiftInput);
		controllerProxy.GetState(0, G_LEFT_TRIGGER_AXIS, lTriggerInput);                           
		controllerProxy.GetState(0, G_RIGHT_TRIGGER_AXIS, rTriggerInput);
		
		float Total_Y_Change = spaceInput - lShiftInput  + rTriggerInput - lTriggerInput;
		
		float newYValue = Total_Y_Change * Camera_Speed * time_span.count();

		cameraMatrix.row4.y += newYValue;

		float wInput = 0;
		float aInput = 0;
		float sInput = 0;
		float dInput = 0;
		float lStickYInput = 0;
		float lStickXInput = 0;

		inputProxy.GetState(G_KEY_W, wInput);
		inputProxy.GetState(G_KEY_A, aInput);
		inputProxy.GetState(G_KEY_S, sInput);
		inputProxy.GetState(G_KEY_D, dInput);

		controllerProxy.GetState(0, G_LY_AXIS, lStickYInput);
		controllerProxy.GetState(0, G_LX_AXIS, lStickXInput);

		float Total_Z_Change = wInput - sInput + lStickYInput;
		float Total_X_Change = dInput - aInput + lStickXInput;

		float PerFrameSpeed = Camera_Speed * time_span.count();

		matrixMath.TranslateLocalF(cameraMatrix, GW::MATH::GVECTORF{ Total_X_Change * PerFrameSpeed, 0, Total_Z_Change * PerFrameSpeed}, cameraMatrix);

		float fov = (65.f / 2.f) / 180.f;
		unsigned int screen_height = 0;
		float mouse_y_delta = 0;
		float mouse_x_delta = 0;
		float r_stick_y_axis = 0;
		float r_stick_x_axis = 0;

		win.GetHeight(screen_height);

		if (inputProxy.GetMouseDelta(mouse_x_delta = 0, mouse_y_delta = 0) 
			!= GW::GReturn::SUCCESS)
		{
			mouse_x_delta = 0;
			mouse_y_delta = 0;
		}

		controllerProxy.GetState(0, G_RY_AXIS, r_stick_y_axis);
		controllerProxy.GetState(0, G_RX_AXIS, r_stick_x_axis);

		float Thumb_Speed = G_PI * time_span.count();
		float total_pitch = fov * mouse_y_delta / static_cast<float>(screen_height) + r_stick_y_axis * -Thumb_Speed;

		GW::MATH::GMATRIXF pitchMatrix;
		matrixMath.RotateXLocalF(GW::MATH::GIdentityMatrixF, total_pitch, pitchMatrix);
		matrixMath.MultiplyMatrixF(pitchMatrix, cameraMatrix, cameraMatrix);

		unsigned int screen_width;
		win.GetClientWidth(screen_width);

		double ar = (screen_width / static_cast<float>(screen_height));
		double total_yaw = fov * ar * mouse_x_delta / screen_width + r_stick_x_axis * Thumb_Speed;
		
		GW::MATH::GMATRIXF yawMatrix;
		matrixMath.RotateYLocalF(GW::MATH::GIdentityMatrixF, total_yaw, yawMatrix);
		GW::MATH::GVECTORF pos = cameraMatrix.row4;
		matrixMath.MultiplyMatrixF(cameraMatrix, yawMatrix, cameraMatrix);
		cameraMatrix.row4 = pos;

		matrixMath.InverseF(cameraMatrix, viewMatrix);
		shaderVars.viewMatrix = viewMatrix;
	}

private:
	VkCommandBuffer GetCurrentCommandBuffer()
	{
		unsigned int currentBuffer;
		vlk.GetSwapchainCurrentImage(currentBuffer);

		VkCommandBuffer commandBuffer;
		vlk.GetCommandBuffer(currentBuffer, (void**)&commandBuffer);
		return commandBuffer;
	}

	void SetUpPipeline(VkCommandBuffer& commandBuffer)
	{
		UpdateWindowDimensions();
		SetViewport(commandBuffer);
		SetScissor(commandBuffer);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		BindVertexBuffers(commandBuffer);
	}

	void SetViewport(const VkCommandBuffer& commandBuffer)
	{
		VkViewport viewport = CreateViewportFromWindowDimensions();
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
	}

	void SetScissor(const VkCommandBuffer& commandBuffer)
	{
		VkRect2D scissor = CreateScissorFromWindowDimensions();
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	}

	void BindVertexBuffers(VkCommandBuffer& commandBuffer)
	{
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexHandle, offsets);
	}


	//Cleanup callback function (passed to VKSurface, will be called when the pipeline shuts down)
	void CleanUp()
	{
		// wait till everything has completed
		vkDeviceWaitIdle(device);

		// Release allocated buffers, shaders & pipeline
		vkDestroyBuffer(device, vertexHandle, nullptr);
		vkFreeMemory(device, vertexData, nullptr);

		for (int i = 0; i < uniformHandle.size(); i++)
		{
			vkDestroyBuffer(device, uniformHandle[i], nullptr);
		}
		for (int i = 0; i < uniformData.size(); i++)
		{
			vkFreeMemory(device, uniformData[i], nullptr);
		}

		vkDestroyDescriptorPool(device, _descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);

		vkDestroyShaderModule(device, vertexShader, nullptr);
		vkDestroyShaderModule(device, fragmentShader, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyPipeline(device, pipeline, nullptr);
	}
};
