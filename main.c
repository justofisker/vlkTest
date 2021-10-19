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

static uint32_t get_graphics_queue_index(VkPhysicalDevice physical_device, VkSurfaceKHR surface) {
    uint32_t queueCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queueCount, NULL);
	assert(queueCount);
	VkQueueFamilyProperties* properties = malloc(sizeof(VkQueueFamilyProperties) * queueCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queueCount, properties);
	for (uint32_t i = 0; i < queueCount; i++)
	{
		if (properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && supports_swapchain(physical_device, surface, i))
		{
			free(properties);
			return i;
		}
	}
	free(properties);
	return UINT32_MAX;
}

static VkDevice create_logical_device(VkPhysicalDevice physical_device, uint32_t graphics_queue_index) {
    const float queue_priorities[] = { 1.0f };
    VkDeviceQueueCreateInfo queueCreateInfo = { 0 };
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = graphics_queue_index;
	queueCreateInfo.queueCount = 1;
	queueCreateInfo.pQueuePriorities = queue_priorities;

    VkDeviceCreateInfo createInfo = { 0 };
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
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

int main() {
    if(volkInitialize() != VK_SUCCESS) {
        fprintf(stderr, "Failed to initialize volk.\n");
        exit(1);
    }
    VkInstance instance = create_instance();
    volkLoadInstance(instance);
    VkPhysicalDevice physical_device = pick_physical_device(instance);
    window_create();
    VkSurfaceKHR surface = create_surface(instance);
    uint32_t graphics_queue_index = get_graphics_queue_index(physical_device, surface);
    VkDevice device = create_logical_device(physical_device, graphics_queue_index);
#ifdef _DEBUG
    VkDebugReportCallbackEXT clb = register_debug_callback(instance);
#endif // _DEBUG

    window_run();

#ifdef _DEBUG
    vkDestroyDebugReportCallbackEXT(instance, clb, NULL);
#endif // _DEBUG
    vkDestroyDevice(device, NULL);
    vkDestroySurfaceKHR(instance, surface, VK_NULL_HANDLE);
    window_destroy();
    vkDestroyInstance(instance, NULL);
    
    return 0;
}
