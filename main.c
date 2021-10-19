#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

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
    createInfo.enabledLayerCount = 1;
    createInfo.ppEnabledLayerNames = debug_layers;
    const char *debug_extensions[] = { VK_EXT_DEBUG_REPORT_EXTENSION_NAME };
    createInfo.enabledExtensionCount = 1;
    createInfo.ppEnabledExtensionNames = debug_extensions;
#else // _DEBUG
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = NULL;
    createInfo.enabledExtensionCount = 0;
    createInfo.ppEnabledExtensionNames = NULL;
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

static VkDevice create_logical_device(VkPhysicalDevice physical_device) {
    VkDeviceCreateInfo createInfo = { 0 };
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.queueCreateInfoCount = 0;
    createInfo.pQueueCreateInfos = NULL;
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = NULL;
    createInfo.enabledExtensionCount = 0;
    createInfo.ppEnabledExtensionNames = NULL;
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
    VkDevice device = create_logical_device(physical_device);
#ifdef _DEBUG
    VkDebugReportCallbackEXT clb = register_debug_callback(instance);
#endif // _DEBUG

#ifdef _DEBUG
    vkDestroyDebugReportCallbackEXT(instance, clb, NULL);
#endif // _DEBUG
    vkDestroyDevice(device, NULL);
    vkDestroyInstance(instance, NULL);

    return 0;
}
