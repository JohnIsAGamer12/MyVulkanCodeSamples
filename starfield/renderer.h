// minimalistic code to draw a single triangle, this is not part of the API.
#include "shaderc/shaderc.h" // needed for compiling shaders at runtime
#ifdef _WIN32 // must use MT platform DLL libraries on windows
#pragma comment(lib, "shaderc_combined.lib") 
#endif

#include <random>
#include <ctime>
#include <vector>

void PrintLabeledDebugString(const char* label, const char* toPrint)
{
	std::cout << label << toPrint << std::endl;
#if defined WIN32 //OutputDebugStringA is a windows-only function 
	OutputDebugStringA(label);
	OutputDebugStringA(toPrint);
#endif
}

#define NUMBER_OF_STARS 10000

// Creation, Rendering & Cleanup
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

	VkBuffer vertexStarHandle = nullptr;
	VkDeviceMemory vertexStarData = nullptr;

	VkShaderModule vertexShader = nullptr;
	VkShaderModule fragmentShader = nullptr;

	VkShaderModule vertexShaderTwo = nullptr;
	VkShaderModule fragmentShaderTwo = nullptr;

	// pipeline settings for drawing (also required)
	VkPipeline pipeline = nullptr;
	VkPipeline starPipeline = nullptr;

	VkPipelineLayout pipelineLayout = nullptr;

	unsigned int windowWidth, windowHeight;
public:
	struct ColorRGB
	{
		float rgba[4];

	public:
		ColorRGB()
		{
			rgba[0] = 0;
			rgba[1] = 0;
			rgba[2] = 0;
			rgba[3] = 0;
		}


		ColorRGB(float _r, float _g, float _b, float _a) 
		{
			rgba[0] = _r;
			rgba[1] = _g;
			rgba[2] = _b;
			rgba[3] = _a;
		}
	};


	struct Vertex
	{
		float x;
		float y;
		ColorRGB rgb;

	public:
		Vertex()
		{
			x = 0;
			y = 0;
			rgb = ColorRGB();
		}

		Vertex(float _x, float _y, ColorRGB _rgb = ColorRGB(1, 1, 1, 1)) :
			x(_x), y(_y), rgb(_rgb) {}
	};

	Renderer(GW::SYSTEM::GWindow _win, GW::GRAPHICS::GVulkanSurface _vlk)
	{
		win = _win;
		vlk = _vlk;

		UpdateWindowDimensions();
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
		// Create the StarField
		float verts2D[NUMBER_OF_STARS * 2];
		srand(time(0));

		std::uniform_real_distribution<float> dist(0.0f, 1.0f);
		std::default_random_engine generator;

		int totalVerts = NUMBER_OF_STARS * 2;
		// loop and populate a 2D vector of Vector2D's
		for (int i = 0; i < totalVerts; i++)
		{
			float Pos = dist(generator) * 2 - 1;

			verts2D[i] = Pos;
		}
		CreateVertexBuffer(vertexHandle, vertexData, &verts2D[0], sizeof(verts2D));

		// Create the Star Vertex for the Star Pipeline
		int totalStarVerts = 6;

		Vertex starVerts[] =
		{
			{ 0.0f, 1.0f, ColorRGB(1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2)) },
			{ -0.25f, 0.35f, ColorRGB(1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2))  },
			{ -0.25f, 0.35f, ColorRGB(1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2)) },
			{ -0.95f, 0.35f, ColorRGB(1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2)) },
			{ -0.95f, 0.35f, ColorRGB(1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2)) },
			{ -0.35f, -0.1f, ColorRGB(1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2)) },
			{ -0.35f, -0.1f, ColorRGB(1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2)) },
			{ -0.65f, -0.85f, ColorRGB(1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1.f) },
			{ -0.65f, -0.85f, ColorRGB(1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2)) },
			{ 0.0f, -0.5f, ColorRGB(1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2)) },
			{ 0.0f, -0.5f, ColorRGB(1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1.f) },
			{ 0.65f, -0.85f, ColorRGB(1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2)) },
			{ 0.65f, -0.85f, ColorRGB(1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2)) },
			{ 0.35f, -0.1f, ColorRGB(1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2)) },
			{ 0.35f, -0.1f, ColorRGB(1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2)) },
			{ 0.95f, 0.35f, ColorRGB(1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2)) },
			{ 0.95f, 0.35f, ColorRGB(1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2)) },
			{ 0.25f, 0.35f, ColorRGB(1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2)) },
			{ 0.25f, 0.35f, ColorRGB(1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2)) },
			{ 0.0f, 1.0f, ColorRGB(1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2),
						 1 - std::pow(1 - (dist(generator)), 2)) }
		};

		CreateVertexBuffer(vertexStarHandle, vertexStarData, &starVerts[0], sizeof(starVerts));
	}

	void CreateVertexBuffer(VkBuffer& buffer, VkDeviceMemory& _deviceMem, const void* data, unsigned int sizeInBytes)
	{
		GvkHelper::create_buffer(physicalDevice, device, sizeInBytes,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &buffer, &_deviceMem);
		// Transfer triangle data to the vertex buffer. (staging would be preferred here)
		GvkHelper::write_to_buffer(device, _deviceMem, data, sizeInBytes);
	}

	void CompileShaders()
	{
		shaderc_compiler_t compiler = shaderc_compiler_initialize();
		shaderc_compile_options_t options = CreateCompileOptions();

		CompileVertexShader(compiler, options, vertexShader, "VertexShader.hlsl");
		CompileFragmentShader(compiler, options, fragmentShader, "FragmentShader.hlsl");

		CompileVertexShader(compiler, options, vertexShaderTwo, "VertexShader2.hlsl");
		CompileFragmentShader(compiler, options, fragmentShaderTwo, "FragmentShader2.hlsl");

		// Free runtime shader compiler resources
		shaderc_compile_options_release(options);
		shaderc_compiler_release(compiler);
	}

	shaderc_compile_options_t CreateCompileOptions()
	{
		shaderc_compile_options_t retval = shaderc_compile_options_initialize();
		shaderc_compile_options_set_source_language(retval, shaderc_source_language_hlsl);
		shaderc_compile_options_set_invert_y(retval, true);
#ifndef NDEBUG
		shaderc_compile_options_set_generate_debug_info(retval);
#endif
		return retval;
	}

	void CompileVertexShader(const shaderc_compiler_t& compiler, const shaderc_compile_options_t& options, VkShaderModule& shaderModule, const char* filepath)
	{
		std::string vertexShaderSource = ReadFileIntoString(filepath);

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
			(char*)shaderc_result_get_bytes(result), &shaderModule);
		shaderc_result_release(result); // done
	}

	void CompileFragmentShader(const shaderc_compiler_t& compiler, const shaderc_compile_options_t& options, VkShaderModule& shaderModule, const char* filepath)
	{
		std::string fragmentShaderSource = ReadFileIntoString(filepath);

		shaderc_compilation_result_t result = shaderc_compile_into_spv( // compile
			compiler, fragmentShaderSource.c_str(), fragmentShaderSource.length(),
			shaderc_fragment_shader, "main.frag", "main", options);

		if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success)// errors?
		{
			PrintLabeledDebugString("Fragment Shader Errors:\n", shaderc_result_get_error_message(result));
			abort();
			return;
		}

		GvkHelper::create_shader_module(device, shaderc_result_get_length(result), // load into Vulkan
			(char*)shaderc_result_get_bytes(result), &shaderModule);
		shaderc_result_release(result); // done
	}

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

		VkPipelineInputAssemblyStateCreateInfo assembly_create_info =
			CreateVkPipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
		VkVertexInputBindingDescription vertex_binding_description =
			CreateVkVertexInputBindingDescription();

		VkVertexInputAttributeDescription vertex_attribute_descriptions[1];
		vertex_attribute_descriptions[0].binding = 0;
		vertex_attribute_descriptions[0].location = 0;
		vertex_attribute_descriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
		vertex_attribute_descriptions[0].offset = 0;

		VkPipelineVertexInputStateCreateInfo input_vertex_info =
			CreateVkPipelineVertexInputStateCreateInfo(
				&vertex_binding_description, 1, vertex_attribute_descriptions, 1);

		VkViewport viewport = CreateViewportFromWindowDimensions();
		VkRect2D scissor = CreateScissorFromWindowDimensions();

		VkPipelineViewportStateCreateInfo viewport_create_info =
			CreateVkPipelineViewportStateCreateInfo(&viewport, 1, &scissor, 1);
		VkPipelineRasterizationStateCreateInfo rasterization_create_info =
			CreateVkPipelineRasterizationStateCreateInfo();
		VkPipelineMultisampleStateCreateInfo multisample_create_info =
			CreateVkPipelineMultisampleStateCreateInfo();
		VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info =
			CreateVkPipelineDepthStencilStateCreateInfo();
		VkPipelineColorBlendAttachmentState color_blend_attachment_state =
			CreateVkPipelineColorBlendAttachmentState();
		VkPipelineColorBlendStateCreateInfo color_blend_create_info =
			CreateVkPipelineColorBlendStateCreateInfo(&color_blend_attachment_state, 1);

		VkDynamicState dynamic_states[2] =
		{
			// By setting these we do not need to re-create the pipeline on Resize
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};

		VkPipelineDynamicStateCreateInfo dynamic_create_info =
			CreateVkPipelineDynamicStateCreateInfo(dynamic_states, 2);

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

		vkCreateGraphicsPipelines(
			device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &pipeline);

		assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
		pipeline_create_info.pInputAssemblyState = &assembly_create_info;

		vertex_binding_description.stride = sizeof(Vertex);
		VkVertexInputAttributeDescription vertex_attribute_descriptions_two[2];
		vertex_attribute_descriptions_two[0].binding = 0;
		vertex_attribute_descriptions_two[0].location = 0;
		vertex_attribute_descriptions_two[0].format = VK_FORMAT_R32G32_SFLOAT;
		vertex_attribute_descriptions_two[0].offset = 0;		

		vertex_attribute_descriptions_two[1].binding = 0;
		vertex_attribute_descriptions_two[1].location = 1;
		vertex_attribute_descriptions_two[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		vertex_attribute_descriptions_two[1].offset = 0;

		VkPipelineVertexInputStateCreateInfo input_vertex_two_info =
			CreateVkPipelineVertexInputStateCreateInfo(
				&vertex_binding_description, 1, vertex_attribute_descriptions_two, 2);

		stage_create_info[0].module = vertexShaderTwo;
		stage_create_info[1].module = fragmentShaderTwo;

		pipeline_create_info.pVertexInputState = &input_vertex_two_info;

		vkCreateGraphicsPipelines(
			device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &starPipeline);
	}

	VkPipelineInputAssemblyStateCreateInfo CreateVkPipelineInputAssemblyStateCreateInfo(VkPrimitiveTopology topology)
	{
		VkPipelineInputAssemblyStateCreateInfo retval = {};
		retval.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		retval.topology = topology;
		retval.primitiveRestartEnable = false;
		return retval;
	}

	VkVertexInputBindingDescription CreateVkVertexInputBindingDescription()
	{
		VkVertexInputBindingDescription retval;
		retval.binding = 0;
		retval.stride = sizeof(float) * 2;
		retval.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return retval;
	}

	VkPipelineVertexInputStateCreateInfo CreateVkPipelineVertexInputStateCreateInfo(
		VkVertexInputBindingDescription* inputBindingDescriptions, uint32_t bindingCount,
		VkVertexInputAttributeDescription* vertexAttributeDescriptions, uint32_t attributeCount)
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

	VkPipelineViewportStateCreateInfo CreateVkPipelineViewportStateCreateInfo(
		VkViewport* viewports, uint32_t viewportCount, VkRect2D* scissors, uint32_t scissorCount)
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
		retval.depthTestEnable = VK_FALSE;
		retval.depthWriteEnable = VK_FALSE;
		retval.depthCompareOp = VK_COMPARE_OP_LESS;
		retval.depthBoundsTestEnable = VK_FALSE;
		retval.minDepthBounds = 0.0f;
		retval.maxDepthBounds = 1.0f;
		retval.stencilTestEnable = VK_FALSE;

		return retval;
	}

	VkPipelineColorBlendAttachmentState CreateVkPipelineColorBlendAttachmentState()
	{
		VkPipelineColorBlendAttachmentState retval;

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

	VkPipelineColorBlendStateCreateInfo CreateVkPipelineColorBlendStateCreateInfo(
		VkPipelineColorBlendAttachmentState* blendAttachmentStates, uint32_t attachmentCount)
	{
		VkPipelineColorBlendStateCreateInfo retval = {};

		retval.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		retval.logicOpEnable = VK_FALSE;
		retval.logicOp = VK_LOGIC_OP_COPY;
		retval.pAttachments = blendAttachmentStates;
		retval.attachmentCount = attachmentCount;
		retval.blendConstants[0] = 0.0f;
		retval.blendConstants[1] = 0.0f;
		retval.blendConstants[2] = 0.0f;
		retval.blendConstants[3] = 0.0f;

		return retval;
	}

	VkPipelineDynamicStateCreateInfo CreateVkPipelineDynamicStateCreateInfo(
		VkDynamicState* dynamicStates, uint32_t dynamicStateCount)
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
		pipeline_layout_create_info.setLayoutCount = 0;
		pipeline_layout_create_info.pSetLayouts = VK_NULL_HANDLE;
		pipeline_layout_create_info.pushConstantRangeCount = 0;
		pipeline_layout_create_info.pPushConstantRanges = nullptr;

		vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &pipelineLayout);
	}

	void BindShutdownCallback()
	{
		/***************** CLEANUP / SHUTDOWN ******************/
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

		SetUpPipeline(commandBuffer);
		vkCmdDraw(commandBuffer, NUMBER_OF_STARS, 1, 0, 0);

		VkDeviceSize offsets[] = { 0 };

		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexStarHandle, offsets);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, starPipeline);
		vkCmdDraw(commandBuffer, 22, 1, 0, 0);
	}
private:
	VkCommandBuffer GetCurrentCommandBuffer()
	{
		unsigned int currentBuffer;
		vlk.GetSwapchainCurrentImage(currentBuffer);

		VkCommandBuffer retval;
		vlk.GetCommandBuffer(currentBuffer, (void**)&retval);
		return retval;
	}

	void SetUpPipeline(const VkCommandBuffer& commandBuffer)
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

	void BindVertexBuffers(const VkCommandBuffer& commandBuffer)
	{
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexHandle, offsets);
	}

	// Cleanup callback function (passed to VkSurface, will be called when the pipeline shuts down)
	void CleanUp()
	{
		// wait till everything has completed
		vkDeviceWaitIdle(device);
		// Release allocated buffers, shaders & pipeline
		vkDestroyBuffer(device, vertexHandle, nullptr);
		vkDestroyBuffer(device, vertexStarHandle, nullptr);
		vkFreeMemory(device, vertexData, nullptr);
		vkFreeMemory(device, vertexStarData, nullptr);
		vkDestroyShaderModule(device, vertexShader, nullptr);
		vkDestroyShaderModule(device, fragmentShader, nullptr);
		vkDestroyShaderModule(device, vertexShaderTwo, nullptr);
		vkDestroyShaderModule(device, fragmentShaderTwo, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyPipeline(device, pipeline, nullptr);
		vkDestroyPipeline(device, starPipeline, nullptr);
	}
};
