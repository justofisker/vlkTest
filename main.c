#define _XOPEN_SOURCE   600
#define _POSIX_C_SOURCE 200112L
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "vlk_window.h"

#define VK_USE_PLATFORM_XLIB_KHR
#define VOLK_IMPLEMENTATION
#include <volk.h>

static VkInstance create_instance() {
    VkApplicationInfo appInfo = { 0 };
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext = NULL;
    appInfo.pApplicationName = "vlkTest";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "vlkTest";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo createInfo = { 0 };
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.pApplicationInfo = &appInfo;
#ifdef _DEBUG
    const char* debug_layers[] = { "VK_LAYER_KHRONOS_validation" };
    createInfo.enabledLayerCount = sizeof(debug_layers) / sizeof(debug_layers[0]);
    createInfo.ppEnabledLayerNames = debug_layers;
    const char *instance_extensions[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XLIB_SURFACE_EXTENSION_NAME, VK_EXT_DEBUG_REPORT_EXTENSION_NAME };
    createInfo.enabledExtensionCount = sizeof(instance_extensions) / sizeof(instance_extensions[0]);
    createInfo.ppEnabledExtensionNames = instance_extensions;
#else // _DEBUG
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = NULL;
    const char *instance_extensions[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XLIB_SURFACE_EXTENSION_NAME };
    createInfo.enabledExtensionCount = sizeof(instance_extensions) / sizeof(instance_extensions[0]);
    createInfo.ppEnabledExtensionNames = instance_extensions;
#endif // _DEBUG

    VkInstance instance = VK_NULL_HANDLE;
    if(vkCreateInstance(&createInfo, NULL, &instance) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan instance.\n");
        exit(1);
    }
    
    return instance;
}

#ifdef _DEBUG
static VkBool32 VKAPI_CALL debug_report_callback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData)
{
	if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
		return VK_FALSE;

	const char* type =
		(flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
		? "ERROR"
		: (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
		? "WARNING"
		: "INFO";

	char message[4096];
	snprintf(message, sizeof(message) / sizeof(message[0]), "%s: %s\n", type, pMessage);

	printf("%s", message);

	//if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
	//	assert(!"Validation error encountered!");

	return VK_FALSE;
}

static VkDebugReportCallbackEXT register_debug_callback(VkInstance instance) {
    VkDebugReportCallbackCreateInfoEXT createInfo = { 0 };
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    createInfo.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT;
	createInfo.pfnCallback = debug_report_callback;

    VkDebugReportCallbackEXT callback = VK_NULL_HANDLE;
    if(vkCreateDebugReportCallbackEXT(instance, &createInfo, NULL, &callback)) {
        fprintf(stderr, "Failed to create Vulkan debug callback.");
        exit(1);
    }

    return callback;
}
#endif

static VkPhysicalDevice pick_physical_device(VkInstance instance) {
    uint32_t count;
    vkEnumeratePhysicalDevices(instance, &count, NULL);
    assert(count);
    VkPhysicalDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice *devices = malloc(sizeof(VkPhysicalDevice) * count);
    vkEnumeratePhysicalDevices(instance, &count, devices);
    for (uint32_t i = 0; i < count; ++i) {
        // TODO: Actually select the best card
        VkPhysicalDeviceProperties prop;
        vkGetPhysicalDeviceProperties(devices[i], &prop);
        printf("Selecting %s \n", prop.deviceName);
        device = devices[i];
        break;
    }
    free(devices);
    return device;
}

static VkSurfaceKHR create_surface(VkInstance instance) {
    VkXlibSurfaceCreateInfoKHR createInfo = { 0 };
    createInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.dpy = window_get_dpy();
    createInfo.window = window_get_window();

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if(vkCreateXlibSurfaceKHR(instance, &createInfo, NULL, &surface) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan Xlib surface.");
        exit(1);
    }
    return surface;
}

static VkBool32 supports_swapchain(VkPhysicalDevice physical_device, VkSurfaceKHR surface, uint32_t index) {
    VkBool32 present_support = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, index, surface, &present_support);
    return present_support;
}

typedef struct Queues
{
    uint32_t graphics_queue;
    uint32_t present_queue;
} Queues;

static Queues get_queue_indices(VkPhysicalDevice physical_device, VkSurfaceKHR surface) {
    uint32_t queueCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queueCount, NULL);
	assert(queueCount);
	VkQueueFamilyProperties* properties = malloc(sizeof(VkQueueFamilyProperties) * queueCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queueCount, properties);
    Queues queues = { UINT32_MAX, UINT32_MAX };
	for (uint32_t i = 0; i < queueCount; ++i)
	{
		if (properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && supports_swapchain(physical_device, surface, i))
		{
			queues.graphics_queue = i;
		}
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &presentSupport);
        if (presentSupport)
        {
            queues.present_queue = i;
        }
        if(queues.graphics_queue != UINT32_MAX && queues.present_queue != UINT32_MAX)
            break;
	}
	free(properties);
	return queues;
}

static VkDevice create_logical_device(VkPhysicalDevice physical_device, Queues queue_indices) {
    const float queue_priorities = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo[2] = { 0 };
    queueCreateInfo[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo[0].queueFamilyIndex = queue_indices.graphics_queue;
    queueCreateInfo[0].queueCount = 1;
    queueCreateInfo[0].pQueuePriorities = &queue_priorities;
    queueCreateInfo[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo[1].queueFamilyIndex = queue_indices.present_queue;
    queueCreateInfo[1].queueCount = 1;
    queueCreateInfo[1].pQueuePriorities = &queue_priorities;

    VkDeviceCreateInfo createInfo = { 0 };
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.queueCreateInfoCount = 1 + (queue_indices.graphics_queue != queue_indices.present_queue);
    createInfo.pQueueCreateInfos = queueCreateInfo;
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = NULL;
    const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	createInfo.enabledExtensionCount = sizeof(deviceExtensions) / sizeof(deviceExtensions[0]);
	createInfo.ppEnabledExtensionNames = deviceExtensions;
    createInfo.pEnabledFeatures = NULL;

    VkDevice device = VK_NULL_HANDLE;
    if(vkCreateDevice(physical_device, &createInfo, NULL, &device) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan logical device.\n");
        exit(0);
    }
    return device;
}

static VkSurfaceFormatKHR get_swapchain_format(VkPhysicalDevice physical_device, VkSurfaceKHR surface) {
    uint32_t count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &count, NULL);
    assert(count);
    VkSurfaceFormatKHR *surface_formats = malloc(sizeof(VkSurfaceFormatKHR) * count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &count, surface_formats);
    VkSurfaceFormatKHR format = surface_formats[0];
    for(uint32_t i = 0; i < count; ++i) {
        if(surface_formats[i].format == VK_FORMAT_B8G8R8A8_SRGB
        && surface_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            format = surface_formats[i];
            break;
        }
    }
    free(surface_formats);
    return format;
}

static VkPresentModeKHR get_present_mode(VkPhysicalDevice physical_device, VkSurfaceKHR surface) {
    uint32_t count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &count, NULL);
    assert(count);
    VkPresentModeKHR *present_modes = malloc(sizeof(VkPresentModeKHR) * count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &count, present_modes);
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (uint32_t i = 0; i < count; ++i) {
        if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            present_mode = present_modes[i];
            break;
        }
    }
    free(present_modes);
    return present_mode;
}

typedef struct SwapchainInfo {
    VkSwapchainKHR swapchain;
    uint32_t image_count;
    VkImage *images;
    VkImageView *image_views;
    VkExtent2D extent;
    VkRenderPass render_pass;
} SwapchainInfo;

static SwapchainInfo create_swapchain(VkDevice device, VkPhysicalDevice physical_device , VkSurfaceKHR surface, VkSurfaceFormatKHR swapchain_format, VkPresentModeKHR present_mode, Queues queue_indices, VkSwapchainKHR old_swapchain) {    
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &capabilities);

    uint32_t image_count = capabilities.minImageCount + 1;
    if(capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount)
        image_count = capabilities.maxImageCount;

    assert(capabilities.currentExtent.width != UINT32_MAX);

    VkSwapchainCreateInfoKHR createInfo = { 0 };
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.surface = surface;
    createInfo.minImageCount = image_count;
    createInfo.imageFormat = swapchain_format.format;
    createInfo.imageColorSpace = swapchain_format.colorSpace;
    createInfo.imageExtent = capabilities.currentExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if(queue_indices.graphics_queue == queue_indices.present_queue) {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = NULL;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = (uint32_t*)&queue_indices;
    }
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = present_mode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = old_swapchain;

    VkSwapchainKHR swapchain;
    if(vkCreateSwapchainKHR(device, &createInfo, NULL, &swapchain) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan swapchain.\n");
        exit(1);
    }
    
    image_count = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &image_count, NULL);
    assert(image_count);
    VkImage *images = malloc(sizeof(VkImage) * image_count);
    vkGetSwapchainImagesKHR(device, swapchain, &image_count, images);

    VkImageView *image_views = malloc(sizeof(VkImageView) * image_count);

    for (size_t i = 0; i < image_count; ++i) {
        VkImageViewCreateInfo createView = { 0 };
        createView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createView.pNext = NULL;
        createView.flags = 0;
        createView.image = images[i];
        createView.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createView.format = swapchain_format.format;
        createView.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createView.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createView.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createView.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createView.subresourceRange.baseMipLevel = 0;
        createView.subresourceRange.levelCount = 1;
        createView.subresourceRange.baseArrayLayer = 0;
        createView.subresourceRange.layerCount = 1;

        if(vkCreateImageView(device, &createView, NULL, &image_views[i]) != VK_SUCCESS) {
            fprintf(stderr, "Failed to create Vulkan swapchain image view.");
            exit(1);
        }
    }

    VkAttachmentDescription color_attachment = { 0 };
    color_attachment.format = swapchain_format.format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref = { 0 };
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;


    VkSubpassDescription subpass = { 0 };
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkSubpassDependency dependency = { 0 };
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo render_pass_info = { 0 };
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    VkRenderPass render_pass;
    if(vkCreateRenderPass(device, &render_pass_info, NULL, &render_pass) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan render pass.\n");
        exit(1);
    }

    return (SwapchainInfo){ swapchain, image_count, images, image_views, createInfo.imageExtent, render_pass };
}

static char *read_file(const char *path, int *length) {
    FILE* file;
    file = fopen(path, "rb");
    if(!file)
    {
        printf("Failed to open %s", path);
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long fsize = ftell(file);
    if(length)
        *length = (int)fsize;
    fseek(file, 0, SEEK_SET);
    char* content = malloc(fsize + 1);
    fread(content, 1, fsize, file);
    fclose(file);
    content[fsize] = 0;
    return content;
}

static VkShaderModule create_shader_module(VkDevice device, const char *code, int length) {
    VkShaderModuleCreateInfo createInfo = { 0 };
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.codeSize = length;
    createInfo.pCode = (const uint32_t*)code;

    VkShaderModule shader_module = VK_NULL_HANDLE;
    if(vkCreateShaderModule(device, &createInfo, NULL, &shader_module) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan shader module\n");
        exit(1);
    }

    return shader_module;
}

typedef struct GraphicPipelineInfo {
    VkPipeline graphics_pipeline;
    VkPipelineLayout pipeline_layout;
} GraphicPipelineInfo;

static GraphicPipelineInfo create_graphics_pipeline(VkDevice device, VkExtent2D swapchain_extent, VkRenderPass render_pass) {
    int vert_shader_code_length;
    char *vert_shader_code = read_file("triangle.vert.spv", &vert_shader_code_length);
    int frag_shader_code_length;
    char *frag_shader_code = read_file("triangle.frag.spv", &frag_shader_code_length);
    VkShaderModule vert_shader_module = create_shader_module(device, vert_shader_code, vert_shader_code_length);
    VkShaderModule frag_shader_module = create_shader_module(device, frag_shader_code, frag_shader_code_length);
    free(vert_shader_code);
    free(frag_shader_code);

    VkPipelineShaderStageCreateInfo vert_shader_stage_info = { 0 };
    vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_shader_stage_info.module = vert_shader_module;
    vert_shader_stage_info.pName = "main";
    VkPipelineShaderStageCreateInfo frag_shader_stage_info = { 0 };
    frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_shader_stage_info.module = frag_shader_module;
    frag_shader_stage_info.pName = "main";
    VkPipelineShaderStageCreateInfo shader_stages[] = { vert_shader_stage_info, frag_shader_stage_info };

    VkPipelineVertexInputStateCreateInfo vertex_input_info = { 0 };
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 0;
    vertex_input_info.pVertexBindingDescriptions = NULL;
    vertex_input_info.vertexAttributeDescriptionCount = 0;
    vertex_input_info.pVertexAttributeDescriptions = NULL;

    VkPipelineInputAssemblyStateCreateInfo input_assembly_info = { 0 };
    input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_info.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport = { 0 };
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)swapchain_extent.width;
    viewport.height = (float)swapchain_extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = { 0 };
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = swapchain_extent;

    VkPipelineViewportStateCreateInfo viewport_info = { 0 };
    viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_info.viewportCount = 1;
    viewport_info.pViewports = &viewport;
    viewport_info.scissorCount = 1;
    viewport_info.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer_info = { 0 };
    rasterizer_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer_info.depthClampEnable = VK_FALSE;
    rasterizer_info.rasterizerDiscardEnable = VK_FALSE;
    rasterizer_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer_info.lineWidth = 1.0f;
    rasterizer_info.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer_info.depthBiasEnable = VK_FALSE;
    rasterizer_info.depthBiasConstantFactor = 0.0f;
    rasterizer_info.depthBiasClamp = 0.0f;
    rasterizer_info.depthBiasSlopeFactor = 0.0f;

    VkPipelineMultisampleStateCreateInfo multisampling_info = { 0 };
    multisampling_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling_info.sampleShadingEnable = VK_FALSE;
    multisampling_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling_info.minSampleShading = 1.0f;
    multisampling_info.pSampleMask = NULL;
    multisampling_info.alphaToCoverageEnable = VK_FALSE;
    multisampling_info.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState color_blend_attachment = { 0 };
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo color_blending = { 0 };
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.logicOp = VK_LOGIC_OP_COPY;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;
    color_blending.blendConstants[0] = 0.0f;
    color_blending.blendConstants[1] = 0.0f;
    color_blending.blendConstants[2] = 0.0f;
    color_blending.blendConstants[3] = 0.0f;

    VkPipelineLayoutCreateInfo pipeline_layout_info = { 0 };
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 0;
    pipeline_layout_info.pSetLayouts = NULL;
    pipeline_layout_info.pushConstantRangeCount = 0;
    pipeline_layout_info.pPushConstantRanges = NULL;

    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    if(vkCreatePipelineLayout(device, &pipeline_layout_info, NULL, &pipeline_layout) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan pipeline layout\n");
        exit(1);  
    }

    VkGraphicsPipelineCreateInfo pipeline_info = { 0 };
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly_info;
    pipeline_info.pViewportState = &viewport_info;
    pipeline_info.pRasterizationState = &rasterizer_info;
    pipeline_info.pMultisampleState = &multisampling_info;
    pipeline_info.pDepthStencilState = NULL; // We dont do this yets
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = NULL; // We dont have any buffers yet
    pipeline_info.layout = pipeline_layout;
    
    pipeline_info.renderPass = render_pass;
    pipeline_info.subpass = 0; // Index of subpass where render_pass is used

    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    VkPipeline graphics_pipeline = VK_NULL_HANDLE;
    if(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &graphics_pipeline) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan graphics pipeline.\n");
        exit(1);
    }

    vkDestroyShaderModule(device, vert_shader_module, NULL);
    vkDestroyShaderModule(device, frag_shader_module, NULL);

    return (GraphicPipelineInfo){ graphics_pipeline, pipeline_layout };
}

static VkFramebuffer *create_framebuffers(VkDevice device, SwapchainInfo swapchain_info) {
    VkFramebuffer *framebuffers = malloc(sizeof(VkFramebuffer) * swapchain_info.image_count);

    for(uint32_t i = 0; i < swapchain_info.image_count; ++i) {
        VkFramebufferCreateInfo createInfo = { 0 };
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.renderPass = swapchain_info.render_pass;
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = &swapchain_info.image_views[i];
        createInfo.width = swapchain_info.extent.width;
        createInfo.height = swapchain_info.extent.height;
        createInfo.layers = 1;

        if(vkCreateFramebuffer(device, &createInfo, NULL, &framebuffers[i]) != VK_SUCCESS) {
            fprintf(stderr, "Failed to create Vulkan framebuffer.\n");
            exit(1);
        }
    }
    
    return framebuffers;
}

static VkCommandPool create_command_pool(VkDevice device, Queues queues) {
    VkCommandPoolCreateInfo pool_info = { 0 };
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = queues.graphics_queue;
    pool_info.flags = 0;

    VkCommandPool command_pool;
    if(vkCreateCommandPool(device, &pool_info, NULL, &command_pool) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan command pool.\n");
        exit(1);
    }

    return command_pool;
}

static VkCommandBuffer *create_command_buffers(VkDevice device, VkCommandPool command_pool, uint32_t count) {
    VkCommandBuffer *command_buffers = malloc(sizeof(VkCommandBuffer) * count);

    VkCommandBufferAllocateInfo alloc_info = { 0 };
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = count;

    if(vkAllocateCommandBuffers(device, &alloc_info, command_buffers) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan command buffers.\n");
        exit(1);
    }
    
    return command_buffers;
}

static VkSemaphore create_semaphore(VkDevice device) {
    VkSemaphoreCreateInfo createInfo = { 0 };
    createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkSemaphore semaphore;
    if(vkCreateSemaphore(device, &createInfo, NULL, &semaphore) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan semaphore.\n");
        exit(1);
    }

    return semaphore;
}

int main() {
    if(volkInitialize() != VK_SUCCESS) {
        fprintf(stderr, "Failed to initialize volk.\n");
        exit(1);
    }
    VkInstance instance = create_instance();
    volkLoadInstanceOnly(instance);
#ifdef _DEBUG
    VkDebugReportCallbackEXT clb = register_debug_callback(instance);
#endif // _DEBUG
    VkPhysicalDevice physical_device = pick_physical_device(instance);
    window_create();
    VkSurfaceKHR surface = create_surface(instance);
    Queues queue_indices = get_queue_indices(physical_device, surface);
    VkDevice device = create_logical_device(physical_device, queue_indices);
    volkLoadDevice(device);
    VkQueue graphics_queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, queue_indices.graphics_queue, 0, &graphics_queue);
    VkQueue present_queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, queue_indices.present_queue, 0, &present_queue);
    VkSurfaceFormatKHR swapchain_format = get_swapchain_format(physical_device, surface);
    VkPresentModeKHR present_mode = get_present_mode(physical_device, surface);
    SwapchainInfo swapchain_info = create_swapchain(device, physical_device, surface, swapchain_format, present_mode, queue_indices, VK_NULL_HANDLE);
    VkSwapchainKHR swapchain = swapchain_info.swapchain;
    GraphicPipelineInfo graphics_pipeline_info = create_graphics_pipeline(device, swapchain_info.extent, swapchain_info.render_pass);
    VkFramebuffer *framebuffers = create_framebuffers(device, swapchain_info);
    VkCommandPool command_pool = create_command_pool(device, queue_indices);
    VkCommandBuffer *command_buffers = create_command_buffers(device, command_pool, swapchain_info.image_count);
    VkSemaphore image_avaliable_semaphore = create_semaphore(device);
    VkSemaphore render_finsihed_semaphore = create_semaphore(device);

    for(uint32_t i = 0; i < swapchain_info.image_count; ++i) {
        VkCommandBufferBeginInfo begin_info = { 0 };
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = 0;
        begin_info.pInheritanceInfo = NULL;

        if(vkBeginCommandBuffer(command_buffers[i], &begin_info) != VK_SUCCESS) {
            fprintf(stderr, "Failed to begin recording to Vulkan command buffer.\n");
            exit(1);
        }

        VkRenderPassBeginInfo render_pass_info = { 0 };
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_info.renderPass = swapchain_info.render_pass;
        render_pass_info.framebuffer = framebuffers[i];
        render_pass_info.renderArea.offset.x = 0;
        render_pass_info.renderArea.offset.y = 0;
        render_pass_info.renderArea.extent = swapchain_info.extent;

        VkClearValue clear_color = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        render_pass_info.clearValueCount = 1;
        render_pass_info.pClearValues = &clear_color;

        vkCmdBeginRenderPass(command_buffers[i], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_info.graphics_pipeline);

        vkCmdDraw(command_buffers[i], 3, 1, 0, 0);

        vkCmdEndRenderPass(command_buffers[i]);

        if(vkEndCommandBuffer(command_buffers[i]) != VK_SUCCESS) {
            fprintf(stderr, "Failed to record to Vulkan command buffer.\n");
            exit(1);
        }
    }

    while(window_run()) {
        uint32_t image_index;
        vkAcquireNextImageKHR(device, swapchain_info.swapchain, UINT64_MAX, image_avaliable_semaphore, VK_NULL_HANDLE, &image_index);

        VkSubmitInfo submit_info = { 0 };
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        
        VkSemaphore wait_semaphore[] = { image_avaliable_semaphore };
        VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = wait_semaphore;
        submit_info.pWaitDstStageMask = wait_stages;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffers[image_index];

        VkSemaphore singal_semaphores[] = { render_finsihed_semaphore };
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = singal_semaphores;

        if(vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS) { 
            fprintf(stderr, "Failed to submit Vulkan queue.\n");
            exit(1);
        }

        VkPresentInfoKHR present_info = { 0 };
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = singal_semaphores;
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &swapchain_info.swapchain;
        present_info.pImageIndices = &image_index;
        present_info.pResults = NULL;

        vkQueuePresentKHR(present_queue, &present_info);

        //usleep(0);
    }
    vkDeviceWaitIdle(device);

    vkDestroySemaphore(device, render_finsihed_semaphore, NULL);
    vkDestroySemaphore(device, image_avaliable_semaphore, NULL);
    vkDestroyCommandPool(device, command_pool, NULL);
    for (uint32_t i = 0; i < swapchain_info.image_count; ++i)
        vkDestroyFramebuffer(device, framebuffers[i], NULL);
    free(framebuffers);
    vkDestroyPipeline(device, graphics_pipeline_info.graphics_pipeline, NULL);
    vkDestroyPipelineLayout(device, graphics_pipeline_info.pipeline_layout, NULL);
    vkDestroyRenderPass(device, swapchain_info.render_pass, NULL);
    for(uint32_t i = 0; i < swapchain_info.image_count; ++i)
        vkDestroyImageView(device, swapchain_info.image_views[i], NULL);
    free(swapchain_info.images);
    free(swapchain_info.image_views);
    vkDestroySwapchainKHR(device, swapchain, NULL);
#ifdef _DEBUG
    vkDestroyDebugReportCallbackEXT(instance, clb, NULL);
#endif // _DEBUG
    vkDestroyDevice(device, NULL);
    vkDestroySurfaceKHR(instance, surface, VK_NULL_HANDLE);
    window_destroy();
    vkDestroyInstance(instance, NULL);
    
    return 0;
}
