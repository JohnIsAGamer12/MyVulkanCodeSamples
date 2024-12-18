#include "shaderc/shaderc.h" // needed for compiling shaders at runtime
#ifdef _WIN32 // must use MT platform DLL libraries on windows
#pragma comment(lib, "shaderc_combined.lib") 
#endif

// define the path to the model files
#define MODEL_PATH "../../bindlesstexturearray/Models/"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "TinyGLTF/tiny_gltf.h"
#include "TextureUtils.h"
#include <chrono>

void PrintLabeledDebugString(const char* label, const char* toPrint)
{
	std::cout << label << toPrint << std::endl;
#if defined WIN32 //OutputDebugStringA is a windows-only function 
	OutputDebugStringA(label);
	OutputDebugStringA(toPrint);
#endif
}

using namespace tinygltf;

class Renderer
{
	// proxy handles
	GW::SYSTEM::GWindow win;
	GW::GRAPHICS::GVulkanSurface vlk;
	GW::CORE::GEventReceiver shutdown;

	// what we need at a minimum to draw a triangle
	VkDevice device = nullptr;
	VkPhysicalDevice physicalDevice = nullptr;
	VkRenderPass renderPass;

	// Buffers
	VkBuffer geometryHandle = nullptr;
	VkDeviceMemory geometryData = nullptr;

	// Texture Data
	struct TextureData
	{
		VkBuffer buffer;
		VkDeviceMemory memory;
		VkImage image;
		VkImageView imageView;
	};
	std::vector<TextureData> textures;

	// Texture Sampler
	std::vector<VkSampler> textureSamplers;

	VkShaderModule vertexShader = nullptr;
	VkShaderModule fragmentShader = nullptr;
	// pipeline settings for drawing (also required)
	VkPipeline pipeline = nullptr;
	VkPipelineLayout pipelineLayout = nullptr;

	unsigned int windowWidth, windowHeight;

	Model model;
	TinyGLTF loader;
	std::string err;
	std::string warn;

	struct SHADER_VARS
	{
		GW::MATH::GMATRIXF worldMatrix;
		GW::MATH::GMATRIXF viewMatrix;
		GW::MATH::GMATRIXF projectionMatrix;
		GW::MATH::GVECTORF sunDir;
		GW::MATH::GVECTORF camPos;
	} shaderVars;

	// Declare Uniform Buffers
	std::vector<VkBuffer> uniformHandle;
	std::vector<VkDeviceMemory> uniformData;
	unsigned int maxFrames;

	// Descriptor Sets
	VkDescriptorSetLayout descriptor_set_layout;
	VkDescriptorSetLayout pixel_descriptor_set_layout;
	VkDescriptorPool _descriptorPool;
	std::vector<VkDescriptorSet> descriptorSets;
	VkDescriptorSet textureDescriptorSet;

	// Camera Matrices
	GW::MATH::GMATRIXF viewMatrix;
	GW::MATH::GMATRIXF projectionMatrix;

	// Declare GInput and GController
	GW::INPUT::GInput input;
	GW::INPUT::GController controller;

public:
	Renderer(GW::SYSTEM::GWindow _win, GW::GRAPHICS::GVulkanSurface _vlk)
	{
		win = _win;
		vlk = _vlk;

		bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, "../../bindlesstexturearray/Models/BarramundiFish2.gltf");

		if (!warn.empty())
			printf("Warn %s\n", warn.c_str());

		if (!err.empty())
			printf("Err: %s\n", err.c_str());

		if (!ret)
			printf("Failed to parse glTF\n");

		textures.resize(model.images.size());
		textureSamplers.resize(model.images.size());
		std::string baseTexturePath;
		std::string ambientMetallicRoughnessTexturePath;
		std::string normalTexturePath;
		std::string fullTexturePath;
		for (size_t i = 0; i < model.images.size(); i++)
		{
			if (baseTexturePath.empty())
			{
				baseTexturePath = model.images[model.materials[model.meshes[0].primitives[0].material].pbrMetallicRoughness.baseColorTexture.index].uri;
				fullTexturePath = MODEL_PATH + baseTexturePath;
			}
			else if (ambientMetallicRoughnessTexturePath.empty())
			{
				ambientMetallicRoughnessTexturePath = model.images[model.materials[model.meshes[0].primitives[0].material].pbrMetallicRoughness.metallicRoughnessTexture.index].uri;
				fullTexturePath = MODEL_PATH + ambientMetallicRoughnessTexturePath;
			}
			else if (normalTexturePath.empty())
			{
				normalTexturePath = model.images[model.materials[model.meshes[0].primitives[0].material].normalTexture.index].uri;
				fullTexturePath = MODEL_PATH + normalTexturePath;
			}

			UploadTextureToGPU(vlk, fullTexturePath, textures[i].buffer, textures[i].memory, textures[i].image, textures[i].imageView);
			CreateSampler(vlk, textureSamplers[i]);
		}

		// Set up the camera
		CreateViewMatrix();
		float aspect = 0.f;
		vlk.GetAspectRatio(aspect);
		projectionMatrix = CreateProjectionMatrix(65.f, aspect, 0.1f, 100.f);

		// Initialize the shader variables
		shaderVars.viewMatrix = viewMatrix;
		shaderVars.projectionMatrix = projectionMatrix;
		shaderVars.worldMatrix = GW::MATH::GIdentityMatrixF;
		shaderVars.sunDir = GW::MATH::GVECTORF{ -1.5f, -1, -2 };

		// Create Proxies for Keyboard and Controller
		input.Create(win);
		controller.Create();

		UpdateWindowDimensions();

		InitializeGraphics();
		BindShutdownCallback();
	}

	void CreateViewMatrix()
	{
		viewMatrix = GW::MATH::GIdentityMatrixF;
		GW::MATH::GMatrix::LookAtLHF(GW::MATH::GVECTORF{ 0.5f, 0.25f, -0.5f }, GW::MATH::GVECTORF{ 0, 0, 0 }, GW::MATH::GVECTORF{ 0, 1, 0 }, viewMatrix);
		shaderVars.camPos = GW::MATH::GVECTORF{ 0, 0, 0 };
	}

	void UpdateCamera()
	{
		std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
		std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
		std::chrono::duration<float> time_span = std::chrono::duration_cast<std::chrono::duration<float>>(t2 - t1);

		// TODO: Part 4c
		GW::MATH::GMATRIXF cameraMatrix = GW::MATH::GIdentityMatrixF;
		GW::MATH::GMatrix::InverseF(viewMatrix, cameraMatrix);
		// TODO: Part 4d
		float spaceInput = 0;
		float lShiftInput = 0;
		float rTriggerInput = 0;
		float lTriggerInput = 0;
		const float Camera_Speed = 300.f;

		input.GetState(G_KEY_SPACE, spaceInput);
		input.GetState(G_KEY_LEFTSHIFT, lShiftInput);
		controller.GetState(0, G_LEFT_TRIGGER_AXIS, lTriggerInput);
		controller.GetState(0, G_RIGHT_TRIGGER_AXIS, rTriggerInput);

		float Total_Y_Change = spaceInput - lShiftInput + rTriggerInput - lTriggerInput;

		float newYValue = Total_Y_Change * Camera_Speed * time_span.count();

		cameraMatrix.row4.y += newYValue;

		// TODO: Part 4e
		float wInput = 0;
		float aInput = 0;
		float sInput = 0;
		float dInput = 0;
		float lStickYInput = 0;
		float lStickXInput = 0;

		input.GetState(G_KEY_W, wInput);
		input.GetState(G_KEY_A, aInput);
		input.GetState(G_KEY_S, sInput);
		input.GetState(G_KEY_D, dInput);

		controller.GetState(0, G_LY_AXIS, lStickYInput);
		controller.GetState(0, G_LX_AXIS, lStickXInput);

		float Total_Z_Change = wInput - sInput + lStickYInput;
		float Total_X_Change = dInput - aInput + lStickXInput;

		float PerFrameSpeed = Camera_Speed * time_span.count();

		GW::MATH::GMatrix::TranslateLocalF(cameraMatrix, GW::MATH::GVECTORF{ Total_X_Change * PerFrameSpeed, 0, Total_Z_Change * PerFrameSpeed }, cameraMatrix);

		float fov = (65.f / 2.f) / 180.f;
		unsigned int screen_height = 0;
		float mouse_y_delta = 0;
		float mouse_x_delta = 0;
		float r_stick_y_axis = 0;
		float r_stick_x_axis = 0;

		win.GetHeight(screen_height);

		if (input.GetMouseDelta(mouse_x_delta = 0, mouse_y_delta = 0)
			!= GW::GReturn::SUCCESS)
		{
			mouse_x_delta = 0;
			mouse_y_delta = 0;
		}

		controller.GetState(0, G_RY_AXIS, r_stick_y_axis);
		controller.GetState(0, G_RX_AXIS, r_stick_x_axis);

		float Thumb_Speed = G_PI * time_span.count();
		float total_pitch = fov * mouse_y_delta / static_cast<float>(screen_height) + r_stick_y_axis * -Thumb_Speed;

		GW::MATH::GMATRIXF pitchMatrix;
		GW::MATH::GMatrix::RotateXLocalF(GW::MATH::GIdentityMatrixF, total_pitch, pitchMatrix);
		GW::MATH::GMatrix::MultiplyMatrixF(pitchMatrix, cameraMatrix, cameraMatrix);

		unsigned int screen_width;
		win.GetClientWidth(screen_width);

		double ar = (screen_width / static_cast<float>(screen_height));
		double total_yaw = fov * ar * mouse_x_delta / screen_width + r_stick_x_axis * Thumb_Speed;

		GW::MATH::GMATRIXF yawMatrix;
		GW::MATH::GMatrix::RotateYLocalF(GW::MATH::GIdentityMatrixF, total_yaw, yawMatrix);
		GW::MATH::GVECTORF pos = cameraMatrix.row4;
		GW::MATH::GMatrix::MultiplyMatrixF(cameraMatrix, yawMatrix, cameraMatrix);
		cameraMatrix.row4 = pos;

		GW::MATH::GMatrix::InverseF(cameraMatrix, viewMatrix);
		shaderVars.viewMatrix = viewMatrix;
		shaderVars.camPos = cameraMatrix.row4;

		projectionMatrix = CreateProjectionMatrix(65.f, static_cast<float>(ar), 0.1f, 100.f);
		shaderVars.projectionMatrix = projectionMatrix;
	}

private:
	GW::MATH::GMATRIXF CreateProjectionMatrix(float fovInDeg, float aspect, float nearPlane, float farPlane)
	{
		GW::MATH::GMATRIXF projectionMatrix = GW::MATH::GIdentityMatrixF;
		GW::MATH::GMatrix::ProjectionVulkanLHF((fovInDeg * 3.14f) / 180.f, aspect, nearPlane, farPlane, projectionMatrix);
		return projectionMatrix;
	}

	void UpdateWindowDimensions()
	{
		win.GetClientWidth(windowWidth);
		win.GetClientHeight(windowHeight);
	}

	void InitializeGraphics()
	{
		GetHandlesFromSurface();
		InitializeGeometry();

		// Function to setup Descritor Sets
		SetupDescriptorSets();

		CompileShaders();
		InitializeGraphicsPipeline();
	}

	void SetupDescriptorSets()
	{
		vlk.GetSwapchainImageCount(maxFrames);

		uniformHandle.resize(maxFrames);
		uniformData.resize(maxFrames);
		descriptorSets.resize(maxFrames);

		// write to the uniform buffer
		for (unsigned int i = 0; i < maxFrames; i++)
		{
			GvkHelper::create_buffer(physicalDevice, device, sizeof(SHADER_VARS),
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformHandle[i], &uniformData[i]);

			GvkHelper::write_to_buffer(device, uniformData[i], &shaderVars, sizeof(SHADER_VARS));
		}

		// setup descriptor pool size
		VkDescriptorPoolSize arrPoolSize[2] = {};
		arrPoolSize[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		arrPoolSize[0].descriptorCount = static_cast<uint32_t>(maxFrames);
		arrPoolSize[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		arrPoolSize[1].descriptorCount = 1;

		// setup descriptor pool create info
		VkDescriptorPoolCreateInfo poolCreateInfo = {};
		poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolCreateInfo.poolSizeCount = 2;
		poolCreateInfo.pPoolSizes = arrPoolSize;
		poolCreateInfo.maxSets = static_cast<uint32_t>(maxFrames) + 1;
		poolCreateInfo.flags = 0;
		poolCreateInfo.pNext = nullptr;

		// create descriptor pool
		vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &_descriptorPool);

		// setup descriptor set layout binding
		VkDescriptorSetLayoutBinding layoutBinding = {};
		layoutBinding.binding = 0;
		layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		layoutBinding.descriptorCount = 1;
		layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		layoutBinding.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutBindingFlagsCreateInfoEXT bindingFlags = {};
		bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
		bindingFlags.bindingCount = 1;

		std::vector<VkDescriptorBindingFlagsEXT> descriptorBindingflags =
		{
			VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT
		};
		bindingFlags.pBindingFlags = descriptorBindingflags.data();

		// setup descriptor set layout info
		VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
		layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutCreateInfo.bindingCount = 1;
		layoutCreateInfo.flags = 0;
		layoutCreateInfo.pNext = &bindingFlags;
		layoutCreateInfo.pBindings = &layoutBinding;

		// create descriptor set layout
		vkCreateDescriptorSetLayout(device, &layoutCreateInfo, nullptr, &descriptor_set_layout);

		// Create descriptor set layout for the pixel shader
		VkDescriptorSetLayoutBinding pixelLayoutBinding = {};
		pixelLayoutBinding.binding = 0;
		pixelLayoutBinding.descriptorCount = textures.size();
		pixelLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pixelLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		pixelLayoutBinding.pImmutableSamplers = nullptr;

		layoutCreateInfo.pBindings = &pixelLayoutBinding;
		vkCreateDescriptorSetLayout(device, &layoutCreateInfo, nullptr, &pixel_descriptor_set_layout);

		// setup descriptor set allocate info
		std::vector<uint32_t> variableDescriptorCounts =
		{
			static_cast<uint32_t>(textures.size())
		};

		VkDescriptorSetVariableDescriptorCountAllocateInfoEXT pixelAllocateInfoExt = {};
		pixelAllocateInfoExt.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT;
		pixelAllocateInfoExt.descriptorSetCount = static_cast<uint32_t>(variableDescriptorCounts.size());
		pixelAllocateInfoExt.pDescriptorCounts = variableDescriptorCounts.data();

		VkDescriptorSetAllocateInfo allocateInfo = {};
		allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocateInfo.descriptorSetCount = 1;
		allocateInfo.pSetLayouts = &pixel_descriptor_set_layout;
		allocateInfo.descriptorPool = _descriptorPool;
		allocateInfo.pNext = &pixelAllocateInfoExt;
		vkAllocateDescriptorSets(device, &allocateInfo, &textureDescriptorSet);

		allocateInfo.pSetLayouts = &descriptor_set_layout;
		allocateInfo.pNext = nullptr;

		// allocate descriptor sets
		VkWriteDescriptorSet writeDescriptorSet = {};
		for (unsigned int i = 0; i < maxFrames; i++)
		{
			vkAllocateDescriptorSets(device, &allocateInfo, &descriptorSets[i]);

			// setup descriptor buffer info
			VkDescriptorBufferInfo bufferInfo = {};
			bufferInfo.buffer = uniformHandle[i];
			bufferInfo.offset = 0;
			bufferInfo.range = VK_WHOLE_SIZE;

			// setup write descriptor set
			writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSet.dstBinding = 0;
			writeDescriptorSet.dstArrayElement = 0;
			writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writeDescriptorSet.descriptorCount = 1;
			writeDescriptorSet.dstSet = descriptorSets[i];
			writeDescriptorSet.pBufferInfo = &bufferInfo;

			// update descriptor set
			vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
		}

		// update the texture descriptor set
		std::vector<VkDescriptorImageInfo> textureDescriptors(textures.size());
		for (size_t i = 0; i < textures.size(); i++)
		{
			textureDescriptors[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			textureDescriptors[i].imageView = textures[i].imageView;
			textureDescriptors[i].sampler = textureSamplers[i];
		}

		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writeDescriptorSet.descriptorCount = static_cast<uint32_t>(textures.size());
		writeDescriptorSet.dstSet = textureDescriptorSet;
		writeDescriptorSet.pImageInfo = textureDescriptors.data();
		vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
	}


	void GetHandlesFromSurface()
	{
		vlk.GetDevice((void**)&device);
		vlk.GetPhysicalDevice((void**)&physicalDevice);
		vlk.GetRenderPass((void**)&renderPass);
	}

	void InitializeGeometry()
	{

		CreateGeometryBuffer(model.buffers[0].data.data(), sizeof(unsigned char) * model.buffers[0].data.size());
	}

	void CreateGeometryBuffer(const void* data, unsigned int sizeInBytes)
	{
		GvkHelper::create_buffer(physicalDevice, device, sizeInBytes,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &geometryHandle, &geometryData);
		// Transfer triangle data to the vertex buffer. (staging would be prefered here)
		GvkHelper::write_to_buffer(device, geometryData, data, sizeInBytes);
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
		shaderc_compile_options_set_invert_y(retval, false);
#ifndef NDEBUG
		shaderc_compile_options_set_generate_debug_info(retval);
#endif
		return retval;
	}

	void CompileVertexShader(const shaderc_compiler_t& compiler, const shaderc_compile_options_t& options)
	{
		std::string vertexShaderSource = ReadFileIntoString("../../bindlesstexturearray/VertexShader.hlsl");

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
		std::string fragmentShaderSource = ReadFileIntoString("../../bindlesstexturearray/FragmentShader.hlsl");

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
		VkVertexInputBindingDescription vertex_binding_description[4] = {};

		// create string for attributes
		std::string attr[] =
		{
			"POSITION",
			"NORMAL",
			"TEXCOORD_0",
			"TANGENT"
		};

		for (int i = 0; i < 4; i++)
		{
			Accessor& Accessor = model.accessors[model.meshes[0].primitives[0].attributes[attr[i]]];
			BufferView& BufferView = model.bufferViews[Accessor.bufferView];

			vertex_binding_description[i] = CreateVkVertexInputBindingDescription(i, Accessor.ByteStride(BufferView));
		}

		VkVertexInputAttributeDescription vertex_attribute_description[4];
		vertex_attribute_description[0].binding = 0;
		vertex_attribute_description[0].location = 0;
		vertex_attribute_description[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertex_attribute_description[0].offset = 0;

		vertex_attribute_description[1].binding = 1;
		vertex_attribute_description[1].location = 1;
		vertex_attribute_description[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertex_attribute_description[1].offset = 0;

		vertex_attribute_description[2].binding = 2;
		vertex_attribute_description[2].location = 2;
		vertex_attribute_description[2].format = VK_FORMAT_R32G32_SFLOAT;
		vertex_attribute_description[2].offset = 0;

		vertex_attribute_description[3].binding = 3;
		vertex_attribute_description[3].location = 3;
		vertex_attribute_description[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		vertex_attribute_description[3].offset = 0;

		VkPipelineVertexInputStateCreateInfo input_vertex_info = CreateVkPipelineVertexInputStateCreateInfo(vertex_binding_description, 4, vertex_attribute_description, 4);
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

	VkVertexInputBindingDescription CreateVkVertexInputBindingDescription(uint32_t _binding, uint32_t _stride)
	{
		VkVertexInputBindingDescription retval = {};
		retval.binding = _binding;
		retval.stride = _stride;
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
		VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};
		pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipeline_layout_create_info.setLayoutCount = 2;

		VkDescriptorSetLayout layouts[2] = { descriptor_set_layout, pixel_descriptor_set_layout };
		pipeline_layout_create_info.pSetLayouts = layouts;
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
		SetUpPipeline(commandBuffer);

		// write to the uniform buffer
		for (int i = 0; i < maxFrames; i++)
		{
			GvkHelper::write_to_buffer(device, uniformData[i], &shaderVars, sizeof(SHADER_VARS));
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[i], 0, nullptr);
		}

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &textureDescriptorSet, 0, nullptr);

		Accessor& indexAccessor = model.accessors[model.meshes[0].primitives[0].indices];

		Accessor& vertexAccessor = model.accessors[model.meshes[0].primitives[0].attributes["POSITION"]];
		BufferView& vertexBufferView = model.bufferViews[vertexAccessor.bufferView];

		vkCmdDrawIndexed(commandBuffer, indexAccessor.count, 1, 0, vertexBufferView.byteOffset + vertexAccessor.byteOffset, 1);
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
		BindGeometryBuffers(commandBuffer);
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

	void BindGeometryBuffers(VkCommandBuffer& commandBuffer)
	{
		// bind the vertex buffer from the geometry

		std::string attr[] =
		{
			"POSITION",
			"NORMAL",
			"TEXCOORD_0",
			"TANGENT"
		};

		for (size_t i = 0; i < 4; i++)
		{
			Accessor& Accessor = model.accessors[model.meshes[0].primitives[0].attributes[attr[i]]];
			BufferView& BufferView = model.bufferViews[Accessor.bufferView];

			VkDeviceSize offset[] = { BufferView.byteOffset + Accessor.byteOffset };
			vkCmdBindVertexBuffers(commandBuffer, i, 1, &geometryHandle, offset);
		}

		// bind the index buffer from the geometry
		Accessor& indexAccessor = model.accessors[model.meshes[0].primitives[0].indices];
		BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
		vkCmdBindIndexBuffer(commandBuffer, geometryHandle, indexBufferView.byteOffset + indexAccessor.byteOffset, VK_INDEX_TYPE_UINT16);
	}

	void CleanUp()
	{
		// wait till everything has completed
		vkDeviceWaitIdle(device);

		// release allocated descriptor sets
		for (unsigned int i = 0; i < maxFrames; i++)
		{
			vkDestroyBuffer(device, uniformHandle[i], nullptr);
			vkFreeMemory(device, uniformData[i], nullptr);
		}
		vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);
		vkDestroyDescriptorSetLayout(device, pixel_descriptor_set_layout, nullptr);

		vkDestroyDescriptorPool(device, _descriptorPool, nullptr);

		// Release allocated buffers, shaders & pipeline
		vkDestroyBuffer(device, geometryHandle, nullptr);
		vkFreeMemory(device, geometryData, nullptr);
		vkDestroyShaderModule(device, vertexShader, nullptr);
		vkDestroyShaderModule(device, fragmentShader, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyPipeline(device, pipeline, nullptr);

		// clean up texture variables
		for (size_t i = 0; i < textures.size(); i++)
		{
			vkDestroyImageView(device, textures[i].imageView, nullptr);
			vkDestroyImage(device, textures[i].image, nullptr);
			vkDestroySampler(device, textureSamplers[i], nullptr);
			vkDestroyBuffer(device, textures[i].buffer, nullptr);
			vkFreeMemory(device, textures[i].memory, nullptr);
		}
	}
};
