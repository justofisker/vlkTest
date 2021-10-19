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

static VkSwapchainKHR create_swapchain(VkDevice device, VkPhysicalDevice physical_device , VkSurfaceKHR surface, VkSurfaceFormatKHR swapchain_format, VkPresentModeKHR present_mode, Queues queue_indices) {
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
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain;
    if(vkCreateSwapchainKHR(device, &createInfo, NULL, &swapchain) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan swapchain.\n");
        exit(1);
    }
    return swapchain;
}

int main() {
    if(volkInitialize() != VK_SUCCESS) {
        fprintf(stderr, "Failed to initialize volk.\n");
        exit(1);
    }
    VkInstance instance = create_instance();
    volkLoadInstance(instance);
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
    VkSwapchainKHR swapchain = create_swapchain(device, physical_device, surface, swapchain_format, present_mode, queue_indices);

    window_run();

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
