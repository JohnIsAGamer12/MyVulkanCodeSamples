// TODO: Part 1b
#include "shaderc/shaderc.h" // needed for compiling shaders at runtime
#ifdef _WIN32 // must use MT platform DLL libraries on windows
#pragma comment(lib, "shaderc_combined.lib") 
#endif

void PrintLabeledDebugString(const char* label, const char* toPrint)
{
	std::cout << label << toPrint << std::endl;
#if defined WIN32 //OutputDebugStringA is a windows-only function 
	OutputDebugStringA(label);
	OutputDebugStringA(toPrint);
#endif
}

// TODO: Part 2b
#include "FSLogo.h"

class Renderer
{
	// TODO: Part 3a

	// proxy handles
	GW::SYSTEM::GWindow win;
	GW::GRAPHICS::GVulkanSurface vlk;
	GW::CORE::GEventReceiver shutdown;

	// what we need at a minimum to draw a triangle
	VkDevice device = nullptr;
	VkPhysicalDevice physicalDevice = nullptr;
	VkRenderPass renderPass;
	VkBuffer vertexHandle = nullptr;
	VkDeviceMemory vertexData = nullptr;
	// TODO: Part 1g
	VkBuffer indexHandle = nullptr;
	VkDeviceMemory indexData = nullptr;
	// TODO: Part 2c
	VkShaderModule vertexShader = nullptr;
	VkShaderModule fragmentShader = nullptr;
	// pipeline settings for drawing (also required)
	VkPipeline pipeline = nullptr;
	VkPipelineLayout pipelineLayout = nullptr;
	// TODO: Part 2d
	std::vector<VkBuffer> uniformHandle;
	std::vector<VkDeviceMemory> uniformData;
	std::vector<VkDescriptorSet> descriptor_set;

	VkDescriptorSetLayout descriptor_set_layout;
	VkDescriptorPool descriptor_pool;
	// TODO: Part 3d
	std::vector<VkBuffer> storageHandle;
	std::vector<VkDeviceMemory> storageData;

	unsigned int windowWidth, windowHeight;

	// TODO: Part 2a
	GW::MATH::GMATRIXF viewMatrix;
	GW::MATH::GMATRIXF projectionMatrix;

	GW::MATH::GVECTORF lightDirection;
	GW::MATH::GVECTORF lightColor;
	GW::MATH::GVECTORF lightAmbient;
	// TODO: Part 2b // TODO: Part 4d
	struct SHADER_SCENE_DATA
	{
		GW::MATH::GVECTORF lightDirection, lightAmbient, lightColor, cameraPos;
		GW::MATH::GMATRIXF viewMatrix, projectionMatrix;
	} shaderSceneData;

	unsigned int maxFrames = 0;
	// TODO: Part 3a
	struct INSTANCE_DATA
	{
		GW::MATH::GMATRIXF worldMatrix;
		OBJ_ATTRIBUTES material;
	};
	std::vector<INSTANCE_DATA> perFrame;

	GW::MATH::GMATRIXF fSLogoMatrix = GW::MATH::GIdentityMatrixF;

public:

	Renderer(GW::SYSTEM::GWindow _win, GW::GRAPHICS::GVulkanSurface _vlk)
	{
		win = _win;
		vlk = _vlk;
		UpdateWindowDimensions();

		// TODO: Part 2a
		float aR;
		vlk.GetAspectRatio(aR);

		GW::MATH::GMatrix::LookAtLHF(GW::MATH::GVECTORF{ 0.75f, 0.25f, -1.5f }, GW::MATH::GVECTORF{ 0.15f, 0.75f, 0.f }, GW::MATH::GVECTORF{ 0, 1, 0 }, viewMatrix);
		GW::MATH::GMatrix::ProjectionVulkanLHF((65.f * G_PI) / 180.f, aR, 0.1f, 100.f, projectionMatrix);
		lightDirection = { -1.f, -1.f, 2.f };
		lightColor = { 0.9f, 0.9f, 1.f, 1.f };
		lightAmbient = { 0.25f, 0.25f, 0.35f };
		// TODO: Part 2b // TODO: Part 4d
		shaderSceneData.viewMatrix = viewMatrix;
		shaderSceneData.projectionMatrix = projectionMatrix;
		shaderSceneData.lightDirection = lightDirection;
		shaderSceneData.lightColor = lightColor;
		shaderSceneData.lightAmbient = lightAmbient;
		shaderSceneData.cameraPos = { 0.75f, 0.25f, -1.5f };
		// TODO: part 3a
		perFrame.resize(FSLogo_meshcount);
		for (int i = 0; i < perFrame.size(); i++)
		{
			perFrame[i].worldMatrix = GW::MATH::GIdentityMatrixF;
			perFrame[i].material = FSLogo_materials[i].attrib;
		}

		InitializeGraphics();
		BindShutdownCallback();
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
		// TODO: Part 1g
		InitializeIndexBuffer();
		// TODO: Part 2d // TODO: Part 3d

		vlk.GetSwapchainImageCount(maxFrames);

		uniformHandle.resize(maxFrames);
		uniformData.resize(maxFrames);
		storageHandle.resize(maxFrames);
		storageData.resize(maxFrames);
		descriptor_set.resize(maxFrames);


		for (int i = 0; i < maxFrames; i++)
		{
			GvkHelper::create_buffer(physicalDevice, device, sizeof(SHADER_SCENE_DATA), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				&uniformHandle[i], &uniformData[i]);
			GvkHelper::write_to_buffer(device, uniformData[i], &shaderSceneData, sizeof(SHADER_SCENE_DATA));

			GvkHelper::create_buffer(physicalDevice, device, sizeof(INSTANCE_DATA) * 2, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				&storageHandle[i], &storageData[i]);
			GvkHelper::write_to_buffer(device, storageData[i], perFrame.data(), sizeof(INSTANCE_DATA) * 2);
		}

		VkDescriptorPoolSize descriptor_poolsize[2] = {};
		descriptor_poolsize[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptor_poolsize[0].descriptorCount = static_cast<uint32_t>(maxFrames);
		descriptor_poolsize[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptor_poolsize[1].descriptorCount = static_cast<uint32_t>(maxFrames);

		VkDescriptorPoolCreateInfo descriptor_pool_createinfo = {};
		descriptor_pool_createinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptor_pool_createinfo.maxSets = static_cast<uint32_t>(maxFrames);
		descriptor_pool_createinfo.poolSizeCount = 2;
		descriptor_pool_createinfo.pPoolSizes = &descriptor_poolsize[0];

		vkCreateDescriptorPool(device, &descriptor_pool_createinfo, nullptr, &descriptor_pool);

		VkDescriptorSetLayoutBinding descriptor_set_layout_binding[2] = {};
		descriptor_set_layout_binding[0].binding = 0;
		descriptor_set_layout_binding[0].descriptorCount = 1;
		descriptor_set_layout_binding[0].pImmutableSamplers = nullptr;
		descriptor_set_layout_binding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptor_set_layout_binding[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		descriptor_set_layout_binding[1].binding = 1;
		descriptor_set_layout_binding[1].descriptorCount = 1;
		descriptor_set_layout_binding[1].pImmutableSamplers = nullptr;
		descriptor_set_layout_binding[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptor_set_layout_binding[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;


		VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info = {};
		descriptor_set_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptor_set_layout_info.bindingCount = 2;
		descriptor_set_layout_info.flags = 0;
		descriptor_set_layout_info.pNext = nullptr;
		descriptor_set_layout_info.pBindings = &descriptor_set_layout_binding[0];

		vkCreateDescriptorSetLayout(device, &descriptor_set_layout_info, nullptr, &descriptor_set_layout);

		VkDescriptorSetAllocateInfo descriptor_set_allocateinfo = {};
		descriptor_set_allocateinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptor_set_allocateinfo.pSetLayouts = &descriptor_set_layout;
		descriptor_set_allocateinfo.descriptorSetCount = 1;
		descriptor_set_allocateinfo.descriptorPool = descriptor_pool;

		VkDescriptorBufferInfo descriptor_uniform_buffer_info[2] = {};
		VkDescriptorBufferInfo descriptor_storage_buffer_info[2] = {};
		for (int i = 0; i < maxFrames; i++)
		{
			vkAllocateDescriptorSets(device, &descriptor_set_allocateinfo, &descriptor_set[i]);

			descriptor_uniform_buffer_info[i].buffer = uniformHandle[i];
			descriptor_uniform_buffer_info[i].range = sizeof(SHADER_SCENE_DATA);

			VkWriteDescriptorSet write_descriptorset[2] = {};
			write_descriptorset[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write_descriptorset[0].dstBinding = 0;
			write_descriptorset[0].descriptorCount = 1;
			write_descriptorset[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			write_descriptorset[0].dstSet = descriptor_set[i];
			write_descriptorset[0].pBufferInfo = &descriptor_uniform_buffer_info[i];


			descriptor_storage_buffer_info[i].buffer = storageHandle[i];
			descriptor_storage_buffer_info[i].range = sizeof(INSTANCE_DATA) * 2;

			write_descriptorset[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write_descriptorset[1].dstBinding = 1;
			write_descriptorset[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			write_descriptorset[1].descriptorCount = 1;
			write_descriptorset[1].dstSet = descriptor_set[i];

			write_descriptorset[1].pBufferInfo = &descriptor_storage_buffer_info[i];

			vkUpdateDescriptorSets(device, 2, &write_descriptorset[0], 0, nullptr);
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
		// TODO: Part 1c
		//float verts[] =
		//{
		//	0,   0.5f,
		//	0.5f, -0.5f,
		//	-0.5f, -0.5f
		//};

		CreateVertexBuffer(&FSLogo_vertices[0], sizeof(FSLogo_vertices));
	}

	void InitializeIndexBuffer()
	{
		CreateIndexBuffer(&FSLogo_indices[0], sizeof(FSLogo_indices));
	}

	void CreateVertexBuffer(const void* data, unsigned int sizeInBytes)
	{
		GvkHelper::create_buffer(physicalDevice, device, sizeInBytes,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &vertexHandle, &vertexData);
		// Transfer triangle data to the vertex buffer. (staging would be prefered here)
		GvkHelper::write_to_buffer(device, vertexData, data, sizeInBytes);
	}

	void CreateIndexBuffer(const void* data, unsigned int sizeInBytes)
	{
		GvkHelper::create_buffer(physicalDevice, device, sizeInBytes,
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &indexHandle, &indexData);
		// transfer indicies data tp the index buffer.
		GvkHelper::write_to_buffer(device, indexData, data, sizeInBytes);
	}

	void CompileShaders()
	{
		// Intialize runtime shader compiler HLSL -> SPIRV
		shaderc_compiler_t compiler = shaderc_compiler_initialize();
		shaderc_compile_options_t options = CreateCompileOptions();

		CompileVertexShader(compiler, options);
		CompilePixelShader(compiler, options);

		// Free runtime shader compiler resources
		shaderc_compile_options_release(options);
		shaderc_compiler_release(compiler);
	}

	shaderc_compile_options_t CreateCompileOptions()
	{
		shaderc_compile_options_t retval = shaderc_compile_options_initialize();
		shaderc_compile_options_set_source_language(retval, shaderc_source_language_hlsl);
		shaderc_compile_options_set_invert_y(retval, false); // TODO: Part 2g
#ifndef NDEBUG
		shaderc_compile_options_set_generate_debug_info(retval);
#endif
		return retval;
	}

	void CompileVertexShader(const shaderc_compiler_t& compiler, const shaderc_compile_options_t& options)
	{
		std::string vertexShaderSource = ReadFileIntoString("../../storage_buffers/VertexShader.hlsl");

		shaderc_compilation_result_t result = shaderc_compile_into_spv( // compile
			compiler, vertexShaderSource.c_str(), vertexShaderSource.length(),
			shaderc_vertex_shader, "main.vert", "main", options);

		if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) // errors?
		{
			PrintLabeledDebugString("Vertex Shader Errors:\n", shaderc_result_get_error_message(result));
			abort();
			return;
		}

		GvkHelper::create_shader_module(device, shaderc_result_get_length(result), // load into Vulkan
			(char*)shaderc_result_get_bytes(result), &vertexShader);

		shaderc_result_release(result); // done
	}

	void CompilePixelShader(const shaderc_compiler_t& compiler, const shaderc_compile_options_t& options)
	{
		std::string fragmentShaderSource = ReadFileIntoString("../../storage_buffers/FragmentShader.hlsl");

		shaderc_compilation_result_t result;

		result = shaderc_compile_into_spv( // compile
			compiler, fragmentShaderSource.c_str(), fragmentShaderSource.length(),
			shaderc_fragment_shader, "main.frag", "main", options);

		if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) // errors?
		{
			PrintLabeledDebugString("Fragment Shader Errors:\n", shaderc_result_get_error_message(result));
			abort();
			return;
		}

		GvkHelper::create_shader_module(device, shaderc_result_get_length(result), // load into Vulkan
			(char*)shaderc_result_get_bytes(result), &fragmentShader);

		shaderc_result_release(result); // done
	}

	void InitializeGraphicsPipeline()
	{
		// Create Pipeline & Layout (Thanks Tiny!)
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

		std::array<VkVertexInputAttributeDescription, 3> vertex_attribute_description;
		vertex_attribute_description[0].binding = 0;
		vertex_attribute_description[0].location = 0;
		vertex_attribute_description[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertex_attribute_description[0].offset = 0;

		vertex_attribute_description[1].binding = 0;
		vertex_attribute_description[1].location = 1;
		vertex_attribute_description[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertex_attribute_description[1].offset = 12;

		vertex_attribute_description[2].binding = 0;
		vertex_attribute_description[2].location = 2;
		vertex_attribute_description[2].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertex_attribute_description[2].offset = 24;

		VkPipelineVertexInputStateCreateInfo input_vertex_info = CreateVkPipelineVertexInputStateCreateInfo(&vertex_binding_description, 1,
			vertex_attribute_description.data(), vertex_attribute_description.size());

		VkViewport viewport = CreateViewportFromWindowDimensions();
		VkRect2D scissor = CreateScissorFromWindowDimensions();
		VkPipelineViewportStateCreateInfo viewport_create_info = CreateVkPipelineViewportStateCreateInfo(&viewport, 1, &scissor, 1);
		VkPipelineRasterizationStateCreateInfo rasterization_create_info = CreateVkPipelineRasterizationStateCreateInfo();
		VkPipelineMultisampleStateCreateInfo multisample_create_info = CreateVkPipelineMultisampleStateCreateInfo();
		VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info = CreateVkPipelineDepthStencilStateCreateInfo();
		VkPipelineColorBlendAttachmentState color_blend_attachment_state = CreateVkPipelineColorBlendAttachmentState();
		VkPipelineColorBlendStateCreateInfo color_blend_create_info = CreateVkPipelineColorBlendStateCreateInfo(&color_blend_attachment_state, 1);

		// Dynamic State 
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

	VkPipelineInputAssemblyStateCreateInfo CreateVkPipelineInputAssemblyStateCreateInfo()
	{
		VkPipelineInputAssemblyStateCreateInfo retval = {};
		retval.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		retval.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		retval.primitiveRestartEnable = false;
		return retval;
	}

	VkVertexInputBindingDescription CreateVkVertexInputBindingDescription()
	{
		// TODO: Part 1e
		VkVertexInputBindingDescription retval = {};
		retval.binding = 0;
		retval.stride = sizeof(OBJ_VERT);
		retval.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return retval;
	}

	VkPipelineVertexInputStateCreateInfo CreateVkPipelineVertexInputStateCreateInfo(
		VkVertexInputBindingDescription* bindingDescriptions, uint32_t bindingCount,
		VkVertexInputAttributeDescription* attributeDescriptions, uint32_t attributeCount)
	{
		VkPipelineVertexInputStateCreateInfo retval = {};
		retval.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		retval.vertexBindingDescriptionCount = bindingCount;
		retval.pVertexBindingDescriptions = bindingDescriptions;
		retval.vertexAttributeDescriptionCount = attributeCount;
		retval.pVertexAttributeDescriptions = attributeDescriptions;
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

	VkPipelineViewportStateCreateInfo CreateVkPipelineViewportStateCreateInfo(VkViewport* viewports, uint32_t viewportCount, VkRect2D* scissors, uint32_t scissorCount)
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

	VkPipelineColorBlendStateCreateInfo CreateVkPipelineColorBlendStateCreateInfo(VkPipelineColorBlendAttachmentState* attachmentStates, uint32_t attachmentCount)
	{
		VkPipelineColorBlendStateCreateInfo retval = {};
		retval.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		retval.logicOpEnable = VK_FALSE;
		retval.logicOp = VK_LOGIC_OP_COPY;
		retval.attachmentCount = attachmentCount;
		retval.pAttachments = attachmentStates;
		retval.blendConstants[0] = 0.0f;
		retval.blendConstants[1] = 0.0f;
		retval.blendConstants[2] = 0.0f;
		retval.blendConstants[3] = 0.0f;
		return retval;
	}

	VkPipelineDynamicStateCreateInfo CreateVkPipelineDynamicStateCreateInfo(VkDynamicState* dynamicStates, uint32_t dynamicStateCount)
	{
		VkPipelineDynamicStateCreateInfo retval = {};
		retval.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		retval.dynamicStateCount = dynamicStateCount;
		retval.pDynamicStates = dynamicStates;
		return retval;
	}

	void CreatePipelineLayout()
	{
		// Descriptor pipeline layout
		// TODO: Part 2d // TODO: Part 3d
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
		// TODO: Part 3i
		GW::MATH::GMATRIXF RotateYMatrix;
		GW::MATH::GMatrix::RotateYLocalF(GW::MATH::GIdentityMatrixF, 0.0001f, RotateYMatrix);
		GW::MATH::GMatrix::MultiplyMatrixF(RotateYMatrix, fSLogoMatrix, fSLogoMatrix);
		perFrame[1].worldMatrix = fSLogoMatrix;
		for (size_t i = 0; i < 2; i++)
		{
			GvkHelper::write_to_buffer(device, storageData[i], perFrame.data(), sizeof(INSTANCE_DATA) * 2);
		}

		// TODO: Part 2a
		VkCommandBuffer commandBuffer = GetCurrentCommandBuffer();
		SetUpPipeline(commandBuffer);
		// TODO: Part 3i
		// TODO: Part 1h
		vkCmdBindIndexBuffer(commandBuffer, indexHandle, 0, VK_INDEX_TYPE_UINT32);
		for (int i = 0; i < static_cast<uint32_t>(maxFrames); i++)
		{
			// TODO: Part 2e
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptor_set[i], 0, nullptr);
		}
		// TODO: Part 3f

		for (size_t i = 0; i < ARRAYSIZE(FSLogo_meshes); i++)
		{
			vkCmdDrawIndexed(commandBuffer, FSLogo_meshes[i].indexCount, 1, FSLogo_meshes[i].indexOffset, 0, i); // TODO: Part 1d 
		}

	}

private:

	VkCommandBuffer GetCurrentCommandBuffer()
	{
		VkCommandBuffer retval;
		unsigned int currentBuffer;
		vlk.GetSwapchainCurrentImage(currentBuffer);
		vlk.GetCommandBuffer(currentBuffer, (void**)&retval);
		return retval;
	}

	void SetUpPipeline(VkCommandBuffer& commandBuffer)
	{
		UpdateWindowDimensions(); // what is the current client area dimensions?
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


	void CleanUp()
	{
		// wait till everything has completed
		vkDeviceWaitIdle(device);
		// Release allocated buffers, shaders & pipeline
		// TODO: Part 1g
		vkDestroyBuffer(device, indexHandle, nullptr);
		vkFreeMemory(device, indexData, nullptr);
		// TODO: Part 2d
		for (int i = 0; i < maxFrames; i++)
		{
			vkDestroyBuffer(device, uniformHandle[i], nullptr);
			vkFreeMemory(device, uniformData[i], nullptr);
		}

		vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);
		// TODO: Part 3d
		for (int i = 0; i < maxFrames; i++)
		{
			vkDestroyBuffer(device, storageHandle[i], nullptr);
			vkFreeMemory(device, storageData[i], nullptr);
		}
		vkDestroyBuffer(device, vertexHandle, nullptr);
		vkFreeMemory(device, vertexData, nullptr);
		vkDestroyShaderModule(device, vertexShader, nullptr);
		vkDestroyShaderModule(device, fragmentShader, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyPipeline(device, pipeline, nullptr);
	}
};
