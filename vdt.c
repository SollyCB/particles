#include "vdt.h"

#ifdef EXE
struct vdt_elem exevdt[VDT_SIZE] = {
    [VDT_EnumeratePhysicalDevices] = {.name = "vkEnumeratePhysicalDevices"},
    [VDT_GetPhysicalDeviceProperties] = {.name = "vkGetPhysicalDeviceProperties"},
    [VDT_GetPhysicalDeviceMemoryProperties] = {.name = "vkGetPhysicalDeviceMemoryProperties"},
    [VDT_GetPhysicalDeviceQueueFamilyProperties] = {.name = "vkGetPhysicalDeviceQueueFamilyProperties"},
    [VDT_GetPhysicalDeviceSurfaceSupportKHR] = {.name = "vkGetPhysicalDeviceSurfaceSupportKHR"},
    [VDT_CreateDevice] = {.name = "vkCreateDevice"},
    [VDT_GetPhysicalDeviceSurfaceCapabilitiesKHR] = {.name = "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"},
    [VDT_GetPhysicalDeviceSurfaceFormatsKHR] = {.name = "vkGetPhysicalDeviceSurfaceFormatsKHR"},
    [VDT_GetPhysicalDeviceSurfacePresentModesKHR] = {.name = "vkGetPhysicalDeviceSurfacePresentModesKHR"},
    
    // Swapchain
    [VDT_CreateSwapchainKHR] = {.name = "vkCreateSwapchainKHR"},
    [VDT_DestroySwapchainKHR] = {.name = "vkDestroySwapchainKHR"},
    [VDT_GetSwapchainImagesKHR] = {.name = "vkGetSwapchainImagesKHR"},
    [VDT_AcquireNextImageKHR] = {.name = "vkAcquireNextImageKHR"},
    [VDT_QueuePresentKHR] = {.name = "vkQueuePresentKHR"},
    
    // Memory
    [VDT_CreateBuffer] = {.name = "vkCreateBuffer"},
    [VDT_DestroyBuffer] = {.name = "vkDestroyBuffer"},
    [VDT_CreateImage] = {.name = "vkCreateImage"},
    [VDT_DestroyImage] = {.name = "vkDestroyImage"},
    [VDT_GetBufferMemoryRequirements] = {.name = "vkGetBufferMemoryRequirements"},
    [VDT_GetImageMemoryRequirements] = {.name = "vkGetImageMemoryRequirements"},
    [VDT_AllocateMemory] = {.name = "vkAllocateMemory"},
    [VDT_MapMemory] = {.name = "vkMapMemory"},
    [VDT_FreeMemory] = {.name = "vkFreeMemory"},
    [VDT_BindBufferMemory] = {.name = "vkBindBufferMemory"},
    [VDT_BindImageMemory] = {.name = "vkBindImageMemory"},
    
    // Shader
    [VDT_CreateImageView] = {.name = "vkCreateImageView"},
    [VDT_DestroyImageView] = {.name = "vkDestroyImageView"},
    [VDT_CreateSampler] = {.name = "vkCreateSampler"},
    [VDT_DestroySampler] = {.name = "vkDestroySampler"},
    [VDT_CreateShaderModule] = {.name = "vkCreateShaderModule"},
    [VDT_DestroyShaderModule] = {.name = "vkDestroyShaderModule"},
    
    // Descriptor
    [VDT_CreateDescriptorSetLayout] = {.name = "vkCreateDescriptorSetLayout"},
    [VDT_DestroyDescriptorSetLayout] = {.name = "vkDestroyDescriptorSetLayout"},
    [VDT_CreatePipelineLayout] = {.name = "vkCreatePipelineLayout"},
    [VDT_DestroyPipelineLayout] = {.name = "vkDestroyPipelineLayout"},
    [VDT_CreateDescriptorPool] = {.name = "vkCreateDescriptorPool"},
    [VDT_DestroyDescriptorPool] = {.name = "vkDestroyDescriptorPool"},
    [VDT_ResetDescriptorPool] = {.name = "vkResetDescriptorPool"},
    [VDT_AllocateDescriptorSets] = {.name = "vkAllocateDescriptorSets"},
    [VDT_UpdateDescriptorSets] = {.name = "vkUpdateDescriptorSets"},
    
    // Pipeline
    [VDT_CreateRenderPass] = {.name = "vkCreateRenderPass"},
    [VDT_DestroyRenderPass] = {.name = "vkDestroyRenderPass"},
    [VDT_CreateFramebuffer] = {.name = "vkCreateFramebuffer"},
    [VDT_DestroyFramebuffer] = {.name = "vkDestroyFramebuffer"},
    [VDT_CreateGraphicsPipelines] = {.name = "vkCreateGraphicsPipelines"},
    [VDT_DestroyPipeline] = {.name = "vkDestroyPipeline"},
    [VDT_CreateSemaphore] = {.name = "vkCreateSemaphore"},
    [VDT_DestroySemaphore] = {.name = "vkDestroySemaphore"},
    [VDT_CreateFence] = {.name = "vkCreateFence"},
    [VDT_DestroyFence] = {.name = "vkDestroyFence"},
    [VDT_WaitForFences] = {.name = "vkWaitForFences"},
    [VDT_ResetFences] = {.name = "vkResetFences"},
    [VDT_GetFenceStatus] = {.name = "vkGetFenceStatus"},
    
    // Command
    [VDT_CreateCommandPool] = {.name = "vkCreateCommandPool"},
    [VDT_DestroyCommandPool] = {.name = "vkDestroyCommandPool"},
    [VDT_AllocateCommandBuffers] = {.name = "vkAllocateCommandBuffers"},
    [VDT_BeginCommandBuffer] = {.name = "vkBeginCommandBuffer"},
    [VDT_EndCommandBuffer] = {.name = "vkEndCommandBuffer"},
    [VDT_ResetCommandPool] = {.name = "vkResetCommandPool"},
    [VDT_FreeCommandBuffers] = {.name = "vkFreeCommandBuffers"},
    
    // Cmd
    [VDT_CmdPipelineBarrier2] = {.name = "vkCmdPipelineBarrier2"},
    [VDT_CmdCopyBuffer] = {.name = "vkCmdCopyBuffer"},
    [VDT_CmdCopyBufferToImage] = {.name = "vkCmdCopyBufferToImage"},
    [VDT_CmdBeginRenderPass2] = {.name = "vkCmdBeginRenderPass2"},
    [VDT_CmdBindPipeline] = {.name = "vkCmdBindPipeline"},
    [VDT_CmdBindDescriptorSets] = {.name = "vkCmdBindDescriptorSets"},
    [VDT_CmdBindVertexBuffers] = {.name = "vkCmdBindVertexBuffers"},
    [VDT_CmdDraw] = {.name = "vkCmdDraw"},
    [VDT_CmdEndRenderPass] = {.name = "vkCmdEndRenderPass"},
    [VDT_CmdSetViewport] = {.name = "vkCmdSetViewport"},
    [VDT_CmdSetScissor] = {.name = "vkCmdSetScissor"},
    
    // Queue
    [VDT_GetDeviceQueue] = {.name = "vkGetDeviceQueue"},
    [VDT_QueueSubmit] = {.name = "vkQueueSubmit"},
};
#else
struct vdt *vdt;
def_create_vdt(create_vdt)
{
    for(u32 i = VDT_INST_START; gpu->inst && !gpu->dev && i < VDT_INST_END; ++i) {
        vdt->table[i].fn = vkGetInstanceProcAddr(gpu->inst, vdt->table[i].name);
        if (!vdt->table[i].fn) {
            log_error("Failed to get pfn for %s", vdt->table[i].name);
            return -1;
        }
    }
    for(u32 i = VDT_DEV_START; gpu->dev && i < VDT_DEV_END; ++i) {
        vdt->table[i].fn = vkGetDeviceProcAddr(gpu->dev, vdt->table[i].name);
        if (!vdt->table[i].fn) {
            log_error("Failed to get pfn for %s", vdt->table[i].name);
            return -1;
        }
    }
    return 0;
}

#endif // ifdef EXE