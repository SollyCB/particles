#ifndef VDT_H
#define VDT_H

#include "gpu.h"

enum {
    /* Instance API */
    VDT_EnumeratePhysicalDevices,
    VDT_GetPhysicalDeviceProperties,
    VDT_GetPhysicalDeviceMemoryProperties,
    VDT_GetPhysicalDeviceQueueFamilyProperties,
    VDT_GetPhysicalDeviceSurfaceSupportKHR,
    VDT_GetPhysicalDeviceSurfaceCapabilitiesKHR,
    VDT_GetPhysicalDeviceSurfaceFormatsKHR,
    VDT_GetPhysicalDeviceSurfacePresentModesKHR,
    VDT_CreateDevice,
    
    VDT_INST_END,
    
    /* Device API */
    
    // Swapchain
    VDT_CreateSwapchainKHR,
    VDT_DestroySwapchainKHR,
    VDT_GetSwapchainImagesKHR,
    VDT_AcquireNextImageKHR,
    VDT_QueuePresentKHR,
    
    // Memory
    VDT_CreateBuffer,
    VDT_DestroyBuffer,
    VDT_CreateImage,
    VDT_DestroyImage,
    VDT_GetBufferMemoryRequirements,
    VDT_GetImageMemoryRequirements,
    VDT_AllocateMemory,
    VDT_MapMemory,
    VDT_FreeMemory,
    VDT_BindBufferMemory,
    VDT_BindImageMemory,
    
    // Shader
    VDT_CreateImageView,
    VDT_DestroyImageView,
    VDT_CreateSampler,
    VDT_DestroySampler,
    VDT_CreateShaderModule,
    VDT_DestroyShaderModule,
    
    // Descriptor
    VDT_CreateDescriptorSetLayout,
    VDT_DestroyDescriptorSetLayout,
    VDT_CreatePipelineLayout,
    VDT_DestroyPipelineLayout,
    VDT_CreateDescriptorPool,
    VDT_DestroyDescriptorPool,
    VDT_ResetDescriptorPool,
    VDT_AllocateDescriptorSets,
    VDT_UpdateDescriptorSets,
    
    // Pipeline
    VDT_CreateRenderPass,
    VDT_DestroyRenderPass,
    VDT_CreateFramebuffer,
    VDT_DestroyFramebuffer,
    VDT_CreateGraphicsPipelines,
    VDT_DestroyPipeline,
    VDT_CreateSemaphore,
    VDT_DestroySemaphore,
    VDT_CreateFence,
    VDT_DestroyFence,
    VDT_WaitForFences,
    VDT_ResetFences,
    VDT_GetFenceStatus,
    
    // Command
    VDT_CreateCommandPool,
    VDT_DestroyCommandPool,
    VDT_AllocateCommandBuffers,
    VDT_BeginCommandBuffer,
    VDT_EndCommandBuffer,
    VDT_ResetCommandPool,
    VDT_FreeCommandBuffers,
    
    // Cmd
    VDT_CmdPipelineBarrier2,
    VDT_CmdCopyBuffer,
    VDT_CmdCopyBufferToImage,
    VDT_CmdBeginRenderPass2,
    VDT_CmdBindPipeline,
    VDT_CmdBindDescriptorSets,
    VDT_CmdBindVertexBuffers,
    VDT_CmdDraw,
    VDT_CmdEndRenderPass,
    VDT_CmdSetViewport,
    VDT_CmdSetScissor,
    
    // Queue
    VDT_GetDeviceQueue,
    VDT_QueueSubmit,
    
    VDT_DEV_END,
    
    // Other meta info
    VDT_INST_START = 0,
    VDT_DEV_START = VDT_INST_END + 1,
    VDT_SIZE = VDT_DEV_END,
};

struct vdt_elem {
    char *name;
    PFN_vkVoidFunction fn;
};

struct vdt {
    struct vdt_elem *table;
};

#ifdef EXE
extern struct vdt_elem exevdt[VDT_SIZE];

#else
extern struct vdt *vdt;

#define def_create_vdt(name) int name(void)
def_create_vdt(create_vdt);

/***************************************************************/

#define GAC NULL

#ifdef DEBUG
#define def_cvk(name) VkResult name(VkResult res, char *file, int line, char *fn)
def_cvk(cvk_fn);
#define cvk(res) cvk_fn(res, __FILE__, __LINE__, __FUNCTION__)
#else
#define cvk(res) res
#endif

#define vdt_call(name) ((PFN_vk ## name)(vdt->table[VDT_ ## name].fn))

static inline VkResult vk_create_inst(VkInstanceCreateInfo *info) {
    return cvk(vkCreateInstance(info, GAC, &gpu->inst));
}

static inline VkResult vk_enum_phys_devs(u32 *cnt, VkPhysicalDevice *devs) {
    return cvk(vdt_call(EnumeratePhysicalDevices)(gpu->inst, cnt, devs));
}

static inline void vk_get_phys_dev_props(VkPhysicalDevice dev, VkPhysicalDeviceProperties *props) {
    vdt_call(GetPhysicalDeviceProperties)(dev, props);
}

static inline void vk_get_phys_dev_memprops(VkPhysicalDevice dev, VkPhysicalDeviceMemoryProperties *props) {
    vdt_call(GetPhysicalDeviceMemoryProperties)(dev, props);
}

static inline void vk_get_phys_devq_fam_props(u32 *cnt, VkQueueFamilyProperties *props) {
    vdt_call(GetPhysicalDeviceQueueFamilyProperties)(gpu->phys_dev, cnt, props);
}

static inline VkResult vk_get_phys_dev_surf_support_khr(u32 qfi, b32 *support) {
    return cvk(vdt_call(GetPhysicalDeviceSurfaceSupportKHR)(gpu->phys_dev, qfi, gpu->surf, support));
}

static inline VkResult vk_create_dev(VkDeviceCreateInfo *ci) {
    return cvk(vdt_call(CreateDevice)(gpu->phys_dev, ci, GAC, &gpu->dev));
}

static inline void vk_get_phys_dev_surf_cap_khr(VkSurfaceCapabilitiesKHR *cap) {
    vdt_call(GetPhysicalDeviceSurfaceCapabilitiesKHR)(gpu->phys_dev, gpu->surf, cap);
}

static inline void vk_get_phys_dev_surf_fmts_khr(u32 *cnt, VkSurfaceFormatKHR *fmts) {
    vdt_call(GetPhysicalDeviceSurfaceFormatsKHR)(gpu->phys_dev, gpu->surf, cnt, fmts);
}

static inline VkResult vk_create_sc_khr(VkSwapchainCreateInfoKHR *ci, VkSwapchainKHR *sc) {
    return cvk(vdt_call(CreateSwapchainKHR)(gpu->dev, ci, GAC, sc));
}

static inline void vk_destroy_sc_khr(VkSwapchainKHR sc) {
    return vdt_call(DestroySwapchainKHR)(gpu->dev, sc, GAC);
}

static inline VkResult vk_get_sc_imgs_khr(u32 *cnt, VkImage *imgs) {
    return cvk(vdt_call(GetSwapchainImagesKHR)(gpu->dev, gpu->sc.handle, cnt, imgs));
}

static inline VkResult vk_acquire_img_khr(VkSemaphore s, VkFence f, u32 *i) {
    return vdt_call(AcquireNextImageKHR)(gpu->dev, gpu->sc.handle, secs_to_ns(1), s, f, i);
}

static inline VkResult vk_qpres(VkPresentInfoKHR *pi) {
    return vdt_call(QueuePresentKHR)(gpu->q[GPU_QI_P].handle, pi);
}

static inline VkResult vk_create_buf(VkBufferCreateInfo *ci, VkBuffer *buf) {
    return cvk(vdt_call(CreateBuffer)(gpu->dev, ci, GAC, buf));
}

static inline void vk_destroy_buf(VkBuffer buf) {
    vdt_call(DestroyBuffer)(gpu->dev, buf, GAC);
}

static inline VkResult vk_create_img(VkImageCreateInfo *ci, VkImage *img) {
    return cvk(vdt_call(CreateImage)(gpu->dev, ci, GAC, img));
}

static inline void vk_destroy_img(VkImage img) {
    vdt_call(DestroyImage)(gpu->dev, img, GAC);
}

static inline void vk_get_buf_memreq(VkBuffer buf, VkMemoryRequirements *mr) {
    vdt_call(GetBufferMemoryRequirements)(gpu->dev, buf, mr);
}

static inline void vk_get_img_memreq(VkImage img, VkMemoryRequirements *mr) {
    vdt_call(GetImageMemoryRequirements)(gpu->dev, img, mr);
}

static inline VkResult vk_alloc_mem(VkMemoryAllocateInfo *ci, VkDeviceMemory *mem) {
    return cvk(vdt_call(AllocateMemory)(gpu->dev, ci, GAC, mem));
}

static inline VkResult vk_map_mem(VkDeviceMemory mem, u64 ofs, u64 sz, void **p) {
    return cvk(vdt_call(MapMemory)(gpu->dev, mem, ofs, sz, 0x0, p));
}

static inline void vk_free_mem(VkDeviceMemory mem) {
    vdt_call(FreeMemory)(gpu->dev, mem, GAC);
}

static inline VkResult vk_bind_img_mem(VkImage img, VkDeviceMemory mem, u64 ofs) {
    return cvk(vdt_call(BindImageMemory)(gpu->dev, img, mem, ofs));
}

static inline VkResult vk_bind_buf_mem(VkBuffer buf, VkDeviceMemory mem, u64 ofs) {
    return cvk(vdt_call(BindBufferMemory)(gpu->dev, buf, mem, ofs));
}

static inline VkResult vk_create_imgv(VkImageViewCreateInfo *ci, VkImageView *view) {
    return cvk(vdt_call(CreateImageView)(gpu->dev, ci, GAC, view));
}

static inline void vk_destroy_imgv(VkImageView view) {
    vdt_call(DestroyImageView)(gpu->dev, view, GAC);
}

static inline VkResult vk_create_sampler(VkSamplerCreateInfo *ci, VkSampler *sampler) {
    return cvk(vdt_call(CreateSampler)(gpu->dev, ci, GAC, sampler));
}

static inline void vk_destroy_sampler(VkSampler sampler) {
    vdt_call(DestroySampler)(gpu->dev, sampler, GAC);
}

static inline VkResult vk_create_shmod(VkShaderModuleCreateInfo *ci, VkShaderModule *mod) {
    return cvk(vdt_call(CreateShaderModule)(gpu->dev, ci, GAC, mod));
}

static inline void vk_destroy_shmod(VkShaderModule mod) {
    vdt_call(DestroyShaderModule)(gpu->dev, mod, GAC);
}

static inline VkResult vk_create_dsl(VkDescriptorSetLayoutCreateInfo *ci, VkDescriptorSetLayout *dsl) {
    return cvk(vdt_call(CreateDescriptorSetLayout)(gpu->dev, ci, GAC, dsl));
}

static inline void vk_destroy_dsl(VkDescriptorSetLayout dsl) {
    vdt_call(DestroyDescriptorSetLayout)(gpu->dev, dsl, GAC);
}

static inline VkResult vk_create_pll(VkPipelineLayoutCreateInfo *ci, VkPipelineLayout *pll) {
    return cvk(vdt_call(CreatePipelineLayout)(gpu->dev, ci, GAC, pll));
}

static inline void vk_destroy_pll(VkPipelineLayout pll) {
    vdt_call(DestroyPipelineLayout)(gpu->dev, pll, GAC);
}

static inline VkResult vk_create_dp(VkDescriptorPoolCreateInfo *ci, VkDescriptorPool *dp) {
    return cvk(vdt_call(CreateDescriptorPool)(gpu->dev, ci, GAC, dp));
}

static inline void vk_destroy_dp(VkDescriptorPool dp) {
    vdt_call(DestroyDescriptorPool)(gpu->dev, dp, GAC);
}

static inline void vk_reset_dp(VkDescriptorPool dp) {
    vdt_call(ResetDescriptorPool)(gpu->dev, dp, 0x0);
}

static inline VkResult vk_alloc_ds(VkDescriptorSetAllocateInfo *ai, VkDescriptorSet *ds) {
    return cvk(vdt_call(AllocateDescriptorSets)(gpu->dev, ai, ds));
}

static inline void vk_update_ds(u32 cnt, VkWriteDescriptorSet *writes) {
    vdt_call(UpdateDescriptorSets)(gpu->dev, cnt, writes, 0, NULL);
}

static inline VkResult vk_create_rp(VkRenderPassCreateInfo *ci, VkRenderPass *rp) {
    return cvk(vdt_call(CreateRenderPass)(gpu->dev, ci, GAC, rp));
}

static inline void vk_destroy_rp(VkRenderPass rp) {
    vdt_call(DestroyRenderPass)(gpu->dev, rp, GAC);
}

static inline VkResult vk_create_fb(VkFramebufferCreateInfo *ci, VkFramebuffer *fb) {
    return cvk(vdt_call(CreateFramebuffer)(gpu->dev, ci, GAC, fb));
}

static inline void vk_destroy_fb(VkFramebuffer fb) {
    vdt_call(DestroyFramebuffer)(gpu->dev, fb, GAC);
}

static inline VkResult vk_create_gpl(u32 cnt, VkGraphicsPipelineCreateInfo *ci, VkPipeline *pl) {
    return cvk(vdt_call(CreateGraphicsPipelines)(gpu->dev, NULL, cnt, ci, GAC, pl));
}

static inline void vk_destroy_pl(VkPipeline pl) {
    vdt_call(DestroyPipeline)(gpu->dev, pl, GAC);
}

static inline VkResult vk_create_sem(VkSemaphore *sem) {
    VkSemaphoreCreateInfo ci = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    return cvk(vdt_call(CreateSemaphore)(gpu->dev, &ci, GAC, sem));
}

static inline void vk_destroy_sem(VkSemaphore sem) {
    vdt_call(DestroySemaphore)(gpu->dev, sem, GAC);
}

static inline VkResult vk_create_fence(bool signalled, VkFence *fence) {
    VkFenceCreateInfo ci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    ci.flags = signalled;
    return cvk(vdt_call(CreateFence)(gpu->dev, &ci, GAC, fence));
}

static inline void vk_destroy_fence(VkFence fence) {
    vdt_call(DestroyFence)(gpu->dev, fence, GAC);
}

static inline void vk_await_fences(u32 cnt, VkFence *fences, bool await_all) {
    // Deliberately ignoring the result
    cvk(vdt_call(WaitForFences)(gpu->dev, cnt, fences, await_all, (u64)10e9));
}

static inline VkResult vk_fence_status(VkFence f) {
    return vdt_call(GetFenceStatus)(gpu->dev, f);
}

static inline void vk_reset_fences(u32 cnt, VkFence *fences) {
    // Deliberately ignoring the result
    cvk(vdt_call(ResetFences)(gpu->dev, cnt, fences));
}

static inline VkResult vk_create_cmdpool(VkCommandPoolCreateInfo *ci, VkCommandPool *pool) {
    return cvk(vdt_call(CreateCommandPool)(gpu->dev, ci, GAC, pool));
}

static inline void vk_destroy_cmdpool(VkCommandPool pool) {
    vdt_call(DestroyCommandPool)(gpu->dev, pool, GAC);
}

static inline VkResult vk_alloc_cmds(u32 ci, u32 cnt) {
    VkCommandBufferAllocateInfo ai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = gpu_cmd(ci).pool;
    ai.commandBufferCount = cnt;
    return cvk(vdt_call(AllocateCommandBuffers)(gpu->dev, &ai, gpu_cmd(ci).bufs + gpu_cmd(ci).buf_cnt));
}

static inline void vk_begin_cmd(VkCommandBuffer cmd, bool one_time) {
    VkCommandBufferBeginInfo bi = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = one_time ? VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT : 0x0,
    };
    // Deliberately assuming that this will never fail by not returning the result.
    // Maybe that is a bad idea, but command buffers are supposed to be basically
    // bottomless, and I will always be submitting a rather tiny number of commands.
    cvk(vdt_call(BeginCommandBuffer)(cmd, &bi));
}

static inline void vk_end_cmd(VkCommandBuffer cmd) {
    // Deliberately assuming that this will never fail by not returning the result.
    // Maybe that is a bad idea, but command buffers are supposed to be basically
    // bottomless, and I will always be submitting a rather tiny number of commands.
    cvk(vdt_call(EndCommandBuffer)(cmd));
}

static inline void vk_reset_cmdpool(VkCommandPool pool, bool release_resources) {
    vdt_call(ResetCommandPool)(gpu->dev, pool, release_resources);
}

static inline void vk_free_cmds(u32 ci) {
    vdt_call(FreeCommandBuffers)(gpu->dev, gpu_cmd(ci).pool, gpu_cmd(ci).buf_cnt, gpu_cmd(ci).bufs);
}

static inline void vk_cmd_pl_barr(VkCommandBuffer cmd, VkDependencyInfo *dep) {
    vdt_call(CmdPipelineBarrier2)(cmd, dep);
}

static inline void vk_cmd_copy_buf_to_img(VkCommandBuffer cmd, VkBuffer buf, VkImage img, u64 ofs, u32 w, u32 h) {
    VkBufferImageCopy r = {
        .bufferOffset = ofs,
        .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,.layerCount = 1},
        .imageExtent = {.width = w, .height = h, .depth = 1},
    };
    vdt_call(CmdCopyBufferToImage)(cmd, buf, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &r);
}

static inline void vk_cmd_bufcpy(VkCommandBuffer cmd, u32 cnt, VkBufferCopy *regs, VkBuffer from, VkBuffer to) {
    vdt_call(CmdCopyBuffer)(cmd, from, to, cnt, regs);
}

static inline void vk_cmd_begin_rp(VkCommandBuffer cmd, VkRenderPassBeginInfo *rbi, VkSubpassBeginInfo *sbi) {
    vdt_call(CmdBeginRenderPass2)(cmd, rbi, sbi);
}

static inline void vk_cmd_bind_pl(VkCommandBuffer cmd, VkPipelineBindPoint bp, VkPipeline pl) {
    vdt_call(CmdBindPipeline)(cmd, bp, pl);
}

static inline void vk_cmd_bind_ds(VkCommandBuffer cmd, VkPipelineBindPoint bp, VkPipelineLayout pll, u32 first, u32 cnt, VkDescriptorSet *ds) {
    vdt_call(CmdBindDescriptorSets)(cmd, bp, pll, first, cnt, ds, 0, NULL);
}

static inline void vk_cmd_bind_vb(VkCommandBuffer cmd, u32 first, u32 cnt, VkBuffer *bufs, u64 *ofs) {
    vdt_call(CmdBindVertexBuffers)(cmd, first, cnt, bufs, ofs);
}

static inline void vk_cmd_draw(VkCommandBuffer cmd, u32 vcnt, u32 icnt) {
    vdt_call(CmdDraw)(cmd, vcnt, icnt, 0, 0);
}

static inline void vk_cmd_end_rp(VkCommandBuffer cmd) {
    vdt_call(CmdEndRenderPass)(cmd);
}

static inline void vk_cmd_set_viewport(VkCommandBuffer cmd, u32 first, u32 cnt, VkViewport *vp) {
    vdt_call(CmdSetViewport)(cmd, first, cnt, vp);
}

static inline void vk_cmd_set_scissor(VkCommandBuffer cmd, u32 first, u32 cnt, VkRect2D *s) {
    vdt_call(CmdSetScissor)(cmd, first, cnt, s);
}

static inline void vk_get_devq(u32 qi, VkQueue *qh) {
    vdt_call(GetDeviceQueue)(gpu->dev, qi, 0, qh);
}

static inline VkResult vk_qsub(VkQueue q, u32 cnt, VkSubmitInfo *si, VkFence fence) {
    return cvk(vdt_call(QueueSubmit)(q, cnt, si, fence));
}

#ifdef DEBUG
def_cvk(cvk_fn)
{
    char *err = "Unknown VkResult (may be an extension)";
    switch(res) {
        case VK_SUCCESS: return 0;
        case VK_NOT_READY: err = "VK_NOT_READY"; break;
        case VK_TIMEOUT: err = "VK_TIMEOUT"; break;
        case VK_EVENT_SET: err = "VK_EVENT_SET"; break;
        case VK_EVENT_RESET: err = "VK_EVENT_RESET"; break;
        case VK_INCOMPLETE: err = "VK_INCOMPLETE"; break;
        case VK_ERROR_OUT_OF_HOST_MEMORY: err = "VK_ERROR_OUT_OF_HOST_MEMORY"; break;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: err = "VK_ERROR_OUT_OF_DEVICE_MEMORY"; break;
        case VK_ERROR_INITIALIZATION_FAILED: err = "VK_ERROR_INITIALIZATION_FAILED"; break;
        case VK_ERROR_DEVICE_LOST: err = "VK_ERROR_DEVICE_LOST"; break;
        case VK_ERROR_MEMORY_MAP_FAILED: err = "VK_ERROR_MEMORY_MAP_FAILED"; break;
        case VK_ERROR_LAYER_NOT_PRESENT: err = "VK_ERROR_LAYER_NOT_PRESENT"; break;
        case VK_ERROR_EXTENSION_NOT_PRESENT: err = "VK_ERROR_EXTENSION_NOT_PRESENT"; break;
        case VK_ERROR_FEATURE_NOT_PRESENT: err = "VK_ERROR_FEATURE_NOT_PRESENT"; break;
        case VK_ERROR_INCOMPATIBLE_DRIVER: err = "VK_ERROR_INCOMPATIBLE_DRIVER"; break;
        case VK_ERROR_TOO_MANY_OBJECTS: err = "VK_ERROR_TOO_MANY_OBJECTS"; break;
        case VK_ERROR_FORMAT_NOT_SUPPORTED: err = "VK_ERROR_FORMAT_NOT_SUPPORTED"; break;
        case VK_ERROR_FRAGMENTED_POOL: err = "VK_ERROR_FRAGMENTED_POOL"; break;
        case VK_ERROR_UNKNOWN: err = "VK_ERROR_UNKNOWN"; break;
        case VK_ERROR_OUT_OF_POOL_MEMORY: err = "VK_ERROR_OUT_OF_POOL_MEMORY"; break;
        case VK_ERROR_INVALID_EXTERNAL_HANDLE: err = "VK_ERROR_INVALID_EXTERNAL_HANDLE"; break;
        case VK_ERROR_FRAGMENTATION: err = "VK_ERROR_FRAGMENTATION"; break;
        case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: err = "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS"; break;
        case VK_PIPELINE_COMPILE_REQUIRED: err = "VK_PIPELINE_COMPILE_REQUIRED"; break;
        default: break;
    }
    log_error("[%s, %u, %s] %s (%i)", file, line, fn, err, (s64)res);
    return res;
}
#endif // DEBUG
#endif // LIB
#endif // VDT_H
