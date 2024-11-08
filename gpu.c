#include "external/stb_truetype.h"

#include "prg.h"
#include "win.h"
#include "gpu.h"
#include "shader.h"

struct gpu *gpu;

u32 frm_i = 0;
static inline void gpu_inc_frame(void)
{
    frm_i = (frm_i + 1) % FRAME_WRAP;
}

char* gpu_mem_names[GPU_MEM_CNT] = {
    [GPU_MI_G] = "Vertex",
    [GPU_MI_T] = "Transfer",
};

char *gpu_cmdq_names[GPU_CMD_CNT] = {
    [GPU_CI_G] = "Graphics",
    [GPU_CI_T] = "Transfer",
};

// map queues that can be submitted to to regular queue index
u32 gpu_ci_to_qi[GPU_CMD_CNT] = {
    [GPU_CI_G] = GPU_QI_G,
    [GPU_CI_T] = GPU_QI_T,
};

internal int gpu_memreq_helper(union gpu_memreq_info info, u32 i, VkMemoryRequirements *mr)
{
    switch(i) {
        case GPU_MI_R:
        case GPU_MI_I: {
            VkImage img;
            if (vk_create_img(info.img, &img))
                break;
            vk_get_img_memreq(img, mr);
            vk_destroy_img(img);
        } return 0;
        
        case GPU_MI_G:
        case GPU_MI_T: {
            VkBuffer buf;
            if (vk_create_buf(info.buf, &buf))
                break;
            vk_get_buf_memreq(buf, mr);
            vk_destroy_buf(buf);
        } return 0;
        
        default: invalid_default_case;
    }
    log_error("Failed to create dummy object for getting memory requirements (%s)", gpu_mem_names[i]);
    return -1;
}

internal u32 gpu_memtype_helper(u32 type_bits, u32 req)
{
    // Ensure that a memory type requiring device features cannot be chosen.
    u32 mem_mask = Max_u32 << (ctz(VK_MEMORY_PROPERTY_PROTECTED_BIT) + 1);
    u64 max_heap = 0;
    
    u32 ret = Max_u32;
    u32 cnt,tz;
    for_bits(tz, cnt, type_bits) {
        if(gpu->memprops.memoryTypes[tz].propertyFlags & mem_mask) {
            continue;
        } else if ((gpu->memprops.memoryTypes[tz].propertyFlags & req) == req &&
                   (gpu->memprops.memoryHeaps[gpu->memprops.memoryTypes[tz].heapIndex].size > max_heap))
        {
            ret = tz;
            max_heap = gpu->memprops.memoryHeaps[gpu->memprops.memoryTypes[tz].heapIndex].size;
        }
    }
    
    return ret;
}

#define gpu_buf_align(sz) align(sz, gpu->props.limits.optimalBufferCopyOffsetAlignment)

internal u64 gpu_buf_alloc(u32 bi, u64 sz)
{
    sz = gpu_buf_align(sz);
    if (gpu->buf[bi].size < gpu->buf[bi].used + sz) {
        log_error("Gpu buffer allocation failed for buffer %u (%s), size requested %u, size remaining %u",
                  bi, gpu_buf_name(bi), sz, gpu->buf[bi].size - gpu->buf[bi].used);
        return Max_u64;
    }
    gpu->buf[bi].used += sz;
    return gpu->buf[bi].used - sz;
}

internal u64 gpu_bufcpy(VkCommandBuffer cmd, u64 ofs, u64 sz, u32 bi_from, u32 bi_to)
{
    VkBufferCopy r;
    r.size = sz;
    r.srcOffset = ofs;
    r.dstOffset = gpu_buf_alloc(bi_to, sz);
    
    if (r.dstOffset == Max_u64) {
        log_error("Failed to copy %u bytes from buffer %u (%s) to buffer %u (%s)",
                  sz, bi_from, gpu_buf_name(bi_from), bi_to, gpu_buf_name(bi_to));
        return Max_u64;
    }
    
    vk_cmd_bufcpy(cmd, 1, &r, gpu->buf[bi_from].handle, gpu->buf[bi_to].handle);
    return r.dstOffset;
}

internal u32 gpu_alloc_cmds(u32 ci, u32 cnt)
{
    if (cnt == 0) return Max_u32;
    
    if (GPU_MAX_CMDS < gpu_cmd(ci).buf_cnt + cnt) {
        log_error("Command buffer allocation overflow for queue %u (%s)", ci, gpu_cmd_name(ci));
        return Max_u32;
    }
    
    if (vk_alloc_cmds(ci, cnt))
        return Max_u32;
    
    gpu_cmd(ci).buf_cnt += cnt;
    return gpu_cmd(ci).buf_cnt - cnt;
}

internal void gpu_dealloc_cmds(u32 ci)
{
    if (gpu_cmd(ci).buf_cnt == 0)
        return;
    vk_free_cmds(ci);
    gpu_cmd(ci).buf_cnt = 0;
}

internal void gpu_await_draw_fence(void)
{
    vk_await_fences(1, &gpu->draw.fence[frm_i], false);
}

internal void gpu_reset_draw_fence(void)
{
    vk_reset_fences(1, &gpu->draw.fence[frm_i]);
}

internal int gpu_create_sc(void)
{
    char msg[128];
    
    for(u32 i=0; i < cl_array_size(gpu->sc.att); ++i) {
        if (gpu->sc.map[i].sem)
            vk_destroy_sem(gpu->sc.map[i].sem);
        
        if (vk_create_sem(&gpu->sc.map[i].sem)) {
            log_error("Failed to create swapchain semaphore %u (I would be very surprised if I ever hit this message)", i);
            while(--i < Max_u32) {
                vk_destroy_sem(gpu->sc.map[i].sem);
                gpu->sc.map[i].sem = VK_NULL_HANDLE;
            }
            return -1;
        }
    }
    
    // NOTE(SollyCB): You have to call this or else you cannot resize to the window dimensions. Weird...
    VkSurfaceCapabilitiesKHR cap;
    vk_get_phys_dev_surf_cap_khr(&cap);
    log_error_if(cap.currentExtent.width != win->dim.w || cap.currentExtent.height != win->dim.h,
                 "Window dimensions do not match those reported by surface capabilities");
    
    VkSwapchainCreateInfoKHR sc_info = gpu->sc.info;
    sc_info.imageExtent = (VkExtent2D) {.width = win->dim.w, .height = win->dim.h};
    
    // I am not actually sure what the appropriate action to take is if this fails.
    // Spec says that oldSwapchain is retired regardless of whether creating a new
    // swapchain succeeds or not, so I do not know what the correct state to return is.
    if (vk_create_sc_khr(&sc_info, &gpu->sc.handle)) {
        strcpy(CLSTR(msg), STR("failed to create swapchain object (TODO I cannot tell if I am handling this correctly, the spec is odd when creation fails but oldSwapchain was a valid handle...)"));
        goto fail_sc;
    }
    vk_destroy_sc_khr(gpu->sc.info.oldSwapchain);
    
    VkImage imgs[SC_MAX_IMGS] = {};
    if (vk_get_sc_imgs_khr(&gpu->sc.img_cnt, imgs)) {
        strcpy(CLSTR(msg), STR("failed to get images"));
        goto fail_imgs;
    }
    
    local_persist VkImageViewCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    
    VkImageView views[SC_MAX_IMGS] = {};
    u32 i;
    for(i=0; i < gpu->sc.img_cnt; ++i) {
        ci.image = imgs[i];
        ci.format = sc_info.imageFormat;
        if (vk_create_imgv(&ci, &views[i])) {
            dbg_strfmt(CLSTR(msg), "failed to create image view %u", i);
            goto fail_views;
        }
    }
    
    gpu->sc.info.oldSwapchain = gpu->sc.handle;
    gpu->sc.info.imageExtent = sc_info.imageExtent;
    gpu->sc.i = 0;
    
    for(i=0; i < gpu->sc.img_cnt; ++i) {
        if (gpu->sc.att[i].view)
            vk_destroy_imgv(gpu->sc.att[i].view);
        gpu->sc.att[i].view = views[i];
        gpu->sc.att[i].img = imgs[i];
    }
    
    return 0;
    
    fail_views:
    while(--i < Max_u32)
        vk_destroy_imgv(views[i]);
    
    fail_imgs:
    // As stated above, I am really not sure what the correct route to take is with regard
    // to swapchain creation failure. The best I can come up with for now is destroy everything
    // and trigger a complete swapchain rebuild.
    vk_destroy_sc_khr(gpu->sc.handle);
    gpu->sc.info.oldSwapchain = NULL;
    gpu->sc.handle = NULL;
    
    fail_sc:
    log_error("Swapchain creation failed - %s", msg);
    return -1;
}

internal int gpu_sc_next_img(void) {
    gpu->sc.i = (gpu->sc.i + 1) % gpu->sc.img_cnt;
    
    while(true) {
        VkResult res = vk_acquire_img_khr(gpu->sc.map[gpu->sc.i].sem, VK_NULL_HANDLE, &gpu->sc.map[gpu->sc.i].i);
        switch(res) {
            case VK_SUCCESS:
            case VK_SUBOPTIMAL_KHR:
            return res;
            
            case VK_TIMEOUT:
            case VK_NOT_READY:
            os_sleep_ms(0);
            break; // loop again
            
            case VK_ERROR_OUT_OF_HOST_MEMORY:
            case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            case VK_ERROR_DEVICE_LOST:
            case VK_ERROR_OUT_OF_DATE_KHR:
            case VK_ERROR_SURFACE_LOST_KHR:
            case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
            log_error("Swapchain image acquisition returned failure code");
            return -1;
        }
    }
    return 0;
}

internal VkShaderModule gpu_create_shader(struct string spv)
{
    VkShaderModuleCreateInfo ci = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = spv.size;
    ci.pCode = (u32*)spv.data;
    
    VkShaderModule ret;
    if (vk_create_shmod(&ci, &ret))
        return VK_NULL_HANDLE;
    
    return ret;
}

internal int gpu_create_dsl(void)
{
    local_persist VkDescriptorSetLayoutBinding b = {
        .binding = SH_SI_SET,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = GPU_DS_CNT,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };
    local_persist VkDescriptorSetLayoutCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &b,
    };
    
    if (vk_create_dsl(&ci, &gpu->dsl))
        return -1;
    
    return 0;
}

internal int gpu_create_pll(void)
{
    VkPipelineLayoutCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &gpu->dsl,
    };
    
    if (vk_create_pll(&ci, &gpu->pll))
        return -1;
    
    return 0;
}

internal int gpu_create_ds(void)
{
    if (gpu->dp == VK_NULL_HANDLE) { // runs once per program
        local_persist VkDescriptorPoolSize sz = {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = GPU_DS_CNT,
        };
        
        local_persist VkDescriptorPoolCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 1,
            .poolSizeCount = 1,
            .pPoolSizes = &sz,
        };
        if (vk_create_dp(&ci, &gpu->dp))
            return -1;
    }
    
    vk_reset_dp(gpu->dp);
    
    VkDescriptorSetAllocateInfo ai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ai.descriptorPool = gpu->dp;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &gpu->dsl;
    
    if (vk_alloc_ds(&ai, &gpu->ds)) {
        log_error("Failed to allocate descriptor sets");
        return -1;
    }
    
#if GPU_DS
    VkDescriptorImageInfo ii[CHT_SZ] = {
        {
            .sampler = gpu->sampler,
            .imageView = gpu->glyph[0].view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        }
    };
    for(u32 i=1; i < CHT_SZ; ++i) {
        ii[i] = ii[0];
        ii[i].imageView = gpu->glyph[i].view;
    }
    
    VkWriteDescriptorSet w = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    w.dstSet = gpu->ds;
    w.dstBinding = 0;
    w.dstArrayElement = 0;
    w.descriptorCount = CHT_SZ;
    w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.pImageInfo = ii;
    
    vk_update_ds(1, &w);
#endif
    
    return 0;
}

internal int gpu_create_rp(void)
{
#if MSAA
    local_persist VkAttachmentDescription a[] = {
        [GPU_FB_AI_MSAA] = {
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE, // TODO(SollyCB): I think this can be store don't care
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        },
        [GPU_FB_AI_SWAP] = {
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        }
    };
    
    local_persist VkAttachmentReference ar[] = {
        [GPU_FB_AI_MSAA] = {
            .attachment = GPU_FB_AI_MSAA,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        },
        [GPU_FB_AI_SWAP] = {
            .attachment = GPU_FB_AI_SWAP,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        }
    };
    
    local_persist VkSubpassDescription s = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &ar[GPU_FB_AI_MSAA],
        .pResolveAttachments = &ar[GPU_FB_AI_SWAP],
    };
    
    local_persist VkSubpassDependency d = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
    };
    
    a[GPU_FB_AI_MSAA].format = gpu->sc.info.imageFormat;
    a[GPU_FB_AI_SWAP].format = gpu->sc.info.imageFormat;
    a[GPU_FB_AI_MSAA].samples = gpu->db.msaa_samples;
#else
    local_persist VkAttachmentDescription a[] = {
        {
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        }
    };
    
    local_persist VkAttachmentReference ar[] = {
        {
            .attachment = 0,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        }
    };
    
    local_persist VkSubpassDescription s = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = ar,
    };
    
    local_persist VkSubpassDependency d = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
    };
    
    a[0].format = gpu->sc.info.imageFormat;
#endif
    
    VkRenderPassCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = cl_array_size(a),
        .pAttachments = a,
        .subpassCount = 1,
        .pSubpasses = &s,
        .dependencyCount = 1,
        .pDependencies = &d,
    };
    
    if (vk_create_rp(&ci, &gpu->rp))
        return -1;
    
    return 0;
}

internal int gpu_next_fb(void)
{
    if (gpu->fb[frm_i] != VK_NULL_HANDLE)
        vk_destroy_fb(gpu->fb[frm_i]);
    
#if MSAA
    VkImageView a[] = {
        [GPU_FB_AI_MSAA] = gpu->db.view[frm_i],
        [GPU_FB_AI_SWAP] = gpu->sc.views[gpu->sc.img_i[gpu->sc.i]],
    };
#else
    VkImageView a[] = {
        sc_att.view,
    };
#endif
    
    VkFramebufferCreateInfo ci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    ci.layers = 1,
    ci.renderPass = gpu->rp,
    ci.width = win->dim.w;
    ci.height = win->dim.h;
    ci.attachmentCount = cl_array_size(a),
    ci.pAttachments = a;
    
    return vk_create_fb(&ci, &gpu->fb[frm_i]);
}

internal int gpu_create_pl(void)
{
    local_persist VkPipelineShaderStageCreateInfo sh[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .pName = SH_ENTRY_POINT,
        },{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pName = SH_ENTRY_POINT,
        }
    };
    
    local_persist VkVertexInputBindingDescription vi_b = {
        .binding = 0,
        .stride = sizeof(*gpu->draw.obj),
        .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE,
    };
    
    local_persist VkVertexInputAttributeDescription vi_a[] = {
        [SH_COL_LOC] = {
            .location = SH_COL_LOC,
            .format = CELL_COL_FMT,
            .offset = offsetof(typeof(*gpu->draw.obj), col),
        },
        [SH_POS_LOC] = {
            .location = SH_POS_LOC,
            .format = CELL_POS_FMT,
            .offset = offsetof(typeof(*gpu->draw.obj), pos),
        },
    };
    
    local_persist VkPipelineVertexInputStateCreateInfo vi = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .vertexAttributeDescriptionCount = cl_array_size(vi_a),
        .pVertexBindingDescriptions = &vi_b,
        .pVertexAttributeDescriptions = vi_a
    };
    
    local_persist VkPipelineInputAssemblyStateCreateInfo ia = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };
    
    local_persist VkViewport view = {
        .x = 0,
        .y = 0,
        .width = 640,
        .height = 480,
        .minDepth = 0,
        .maxDepth = 1,
    };
    local_persist VkRect2D scissor = {
        .offset = {.x = 0, .y = 0},
        .extent = {.width = 640, .height = 480},
    };
    
    local_persist VkPipelineViewportStateCreateInfo vp = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
        .pViewports = &view,
        .pScissors = &scissor,
    };
    
    local_persist VkPipelineRasterizationStateCreateInfo rs = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .lineWidth = 1.0f,
    };
    
    local_persist VkPipelineMultisampleStateCreateInfo ms = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_TRUE,
        .minSampleShading = 0.2f,
    };
    
#if MSAA
    ms.rasterizationSamples = gpu->db.msaa_samples;
#else
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
#endif
    
    local_persist VkPipelineDepthStencilStateCreateInfo ds = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    };
    
    local_persist VkPipelineColorBlendAttachmentState cba = {
        .colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|
            VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT,
    };
    
    local_persist VkPipelineColorBlendStateCreateInfo cb = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &cba,
    };
    
    local_persist VkDynamicState dyn_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    
    local_persist VkPipelineDynamicStateCreateInfo dy = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = cl_array_size(dyn_states),
        .pDynamicStates = dyn_states,
    };
    
    sh[0].module = gpu->sh.vert;
    sh[1].module = gpu->sh.frag;
    
    view.width = (f32)win->dim.w;
    view.height = (f32)win->dim.h;
    scissor.extent.width = win->dim.w;
    scissor.extent.height = win->dim.h;
    
    local_persist VkGraphicsPipelineCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = cl_array_size(sh),
        .pStages = sh,
        .pVertexInputState = &vi,
        .pInputAssemblyState = &ia,
        .pViewportState = &vp,
        .pRasterizationState = &rs,
        .pMultisampleState = &ms,
        .pDepthStencilState = &ds,
        .pColorBlendState = &cb,
        .pDynamicState = &dy,
    };
    
    ci.layout = gpu->pll;
    ci.renderPass = gpu->rp;
    
    VkPipeline pl = VK_NULL_HANDLE;
    if (vk_create_gpl(1, &ci, &pl))
        return -1;
    
    if (gpu->pl)
        vk_destroy_pl(gpu->pl);
    gpu->pl = pl;
    
    return 0;
}

/*******************************************************************/
// Header functions

def_create_gpu(create_gpu)
{
    {
        u32 ver;
        if (vkEnumerateInstanceVersion(&ver) == VK_ERROR_OUT_OF_HOST_MEMORY) {
            log_error("Failed to enumerate vulkan instance version (note that this can only happen due to the loader or enabled layers).");
            return -1;
        }
        if (VK_API_VERSION_MAJOR(ver) != 1 || VK_API_VERSION_MINOR(ver) < 3) {
            log_error("Vulkan instance version is not 1.3 or higher");
            return -1;
        }
        
        VkApplicationInfo ai = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
        ai.pApplicationName = "name";
        ai.applicationVersion = 0;
        ai.pEngineName = "engine";
        ai.engineVersion = 0;
        ai.apiVersion = VK_API_VERSION_1_3;
        
#define MAX_EXTENSION_COUNT 8
        u32 ext_count = 0;
        char *exts[MAX_EXTENSION_COUNT];
        win_inst_exts(&ext_count, NULL); assert(ext_count <= MAX_EXTENSION_COUNT);
        win_inst_exts(&ext_count, exts);
        
        VkInstanceCreateInfo ci = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        ci.pApplicationInfo = &ai;
        ci.enabledExtensionCount = ext_count;
        ci.ppEnabledExtensionNames = exts;
        
        if (vk_create_inst(&ci))
            return -1;
        
        if (win_create_surf())
            return -1;
#undef MAX_EXTENSION_COUNT
    }
    
    create_vdt(); // initialize instance api calls
    
    {
#define MAX_DEVICE_COUNT 8
        u32 cnt;
        VkPhysicalDevice pd[MAX_DEVICE_COUNT];
        vk_enum_phys_devs(&cnt, NULL); assert(cnt <= MAX_DEVICE_COUNT);
        vk_enum_phys_devs(&cnt, pd);
        
        VkPhysicalDeviceProperties props[MAX_DEVICE_COUNT];
        u32 discrete   = Max_u32;
        u32 integrated = Max_u32;
        
        for(u32 i=0; i < cnt; ++i) {
            vk_get_phys_dev_props(pd[i], props);
            if (props[i].deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                discrete = i;
                break;
            } else if (props[i].deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
                integrated = i;
            }
        }
        
        if (discrete != Max_u32) {
            gpu->phys_dev = pd[discrete];
            gpu->props = props[discrete];
        } else if (integrated != Max_u32) {
            gpu->phys_dev = pd[integrated];
            gpu->props = props[integrated];
        } else {
            log_error("Failed to find device with appropriate type");
            return -1;
        }
#undef MAX_DEVICE_COUNT
    }
    
    {
#define MAX_QUEUE_COUNT 8
        u32 cnt;
        VkQueueFamilyProperties fp[MAX_QUEUE_COUNT];
        vk_get_phys_devq_fam_props(&cnt, NULL); assert(cnt <= MAX_QUEUE_COUNT);
        vk_get_phys_devq_fam_props(&cnt, fp);
        
        u32 qi[GPU_Q_CNT];
        memset(qi, 0xff, sizeof(qi));
        for(u32 i=0; i < cnt; ++i) {
            b32 surf;
            vk_get_phys_dev_surf_support_khr(i, &surf);
            if (surf && qi[GPU_QI_P] == Max_u32) {
                qi[GPU_QI_P] = i;
            }
            if ((fp[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
                (fp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                qi[GPU_QI_G] == Max_u32)
            {
                qi[GPU_QI_G] = i;
            }
            if ((fp[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
                !(fp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                qi[GPU_QI_T] == Max_u32)
            {
                qi[GPU_QI_T] = i;
            }
        }
        
        if (qi[GPU_QI_T] == Max_u32)
            qi[GPU_QI_T] = qi[GPU_QI_G];
        
        if (qi[GPU_QI_G] == Max_u32 || qi[GPU_QI_P] == Max_u32) {
            log_error_if(qi[GPU_QI_G] == Max_u32,
                         "physical device %s does not support graphics operations",
                         gpu->props.deviceName);
            log_error_if(qi[GPU_QI_P] == Max_u32,
                         "physical device %s does not support present operations",
                         gpu->props.deviceName);
            return -1;
        }
        
        float priority = 1;
        VkDeviceQueueCreateInfo qci[GPU_Q_CNT] = {
            {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = qi[0],
                .queueCount = 1,
                .pQueuePriorities = &priority,
            }
        };
        u32 qc = 1;
        {
            u32 set = 1;
            for(u32 i=1; i < GPU_Q_CNT; ++i) {
                if (!set_test(set, qi[i])) {
                    qci[qc] = qci[0];
                    qci[qc].queueFamilyIndex = qi[i];
                    qc += 1;
                    set = (u32)set_add(set, i);
                }
            }
        }
        
        char *ext_names[] = {
            "VK_KHR_swapchain",
        };
        
        VkPhysicalDeviceVulkan12Features feat12 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
        };
        
        VkPhysicalDeviceVulkan13Features feat13 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .pNext = &feat12,
            .synchronization2 = VK_TRUE,
        };
        
        VkPhysicalDeviceFeatures df = {};
        df.sampleRateShading = VK_TRUE;
        
        VkDeviceCreateInfo ci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        ci.pNext = &feat13;
        ci.queueCreateInfoCount = qc;
        ci.pQueueCreateInfos = qci;
        ci.enabledExtensionCount = cl_array_size(ext_names);
        ci.ppEnabledExtensionNames = ext_names;
        ci.pEnabledFeatures = &df;
        
        if (vk_create_dev(&ci))
            return -1;
        
        gpu->q_cnt = qc;
        for(u32 i=0; i < GPU_Q_CNT; ++i)
            gpu->q[i].i = qi[i];
        
        create_vdt(); // initialize device api calls
        
        {
            vk_get_devq(gpu->q[0].i, &gpu->q[0].handle);
            u32 set = 1;
            u8 map[MAX_QUEUE_COUNT];
            for(u32 i=1; i < GPU_Q_CNT; ++i) {
                if (!set_test(set, gpu->q[i].i)) {
                    vk_get_devq(gpu->q[i].i, &gpu->q[i].handle);
                    map[gpu->q[i].i] = (u8)i;
                    set = (u32)set_add(set, gpu->q[i].i);
                } else {
                    gpu->q[i].handle = gpu->q[map[gpu->q[i].i]].handle;
                }
            }
        }
#undef MAX_QUEUE_COUNT
    }
    
    {
        VkSurfaceCapabilitiesKHR cap;
        vk_get_phys_dev_surf_cap_khr(&cap);
        
        log_error_if(cap.currentExtent.width != win->dim.w || cap.currentExtent.height != win->dim.h,
                     "Window dimensions do not match those reported by surface capabilities");
        
        if (cap.minImageCount > SC_MAX_IMGS) {
            log_error("SC_MAX_IMGS is smaller that minimum required swapchain images");
            return -1;
        }
        if (cap.maxImageCount < SC_MIN_IMGS && cap.maxImageCount != 0) {
            log_error("SC_MIN_IMGS is greater than maximum available swapchain images");
            return -1;
        }
        
        gpu->sc.img_cnt = cap.minImageCount < SC_MIN_IMGS ? SC_MIN_IMGS : cap.minImageCount;
        
        u32 fmt_cnt = 1;
        VkSurfaceFormatKHR fmt;
        vk_get_phys_dev_surf_fmts_khr(&fmt_cnt, &fmt);
        
        gpu->sc.info = (typeof(gpu->sc.info)) {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        gpu->sc.info.surface = gpu->surf;
        gpu->sc.info.preTransform = cap.currentTransform;
        gpu->sc.info.imageExtent = cap.currentExtent;
        gpu->sc.info.imageFormat = fmt.format;
        gpu->sc.info.imageColorSpace = fmt.colorSpace;
        gpu->sc.info.minImageCount = gpu->sc.img_cnt;
        gpu->sc.info.imageArrayLayers = 1;
        gpu->sc.info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        gpu->sc.info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        gpu->sc.info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        gpu->sc.info.clipped = VK_TRUE;
        gpu->sc.info.queueFamilyIndexCount = 1;
        gpu->sc.info.pQueueFamilyIndices = &gpu->q[GPU_QI_P].i;
        gpu->sc.info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        
        if (gpu_create_sc())
            return -1;
    }
    
    gpu_create_sh();
    gpu_create_dsl();
    gpu_create_pll();
    gpu_create_ds();
    gpu_create_rp();
    gpu_create_pl();
    
    return 0;
}

def_gpu_create_sh(gpu_create_sh)
{
    struct string src;
    src.size = read_file(SH_SRC_URI, NULL, 0);
    src.data = salloc(MT, src.size);
    read_file(SH_SRC_URI, src.data, src.size);
    
    // You have to look at the structure of shader.h to understand what is happening here.
    // It is basically chopping the file up into vertex and fragment source code segments
    // based on the position of some marker defines in the file.
    u32 ofs = strfind(STR("SH_BEGIN"), src) + (u32)strlen("SH_BEGIN") + 1;
    u32 sz = strfind(STR("SH_END"), src) + (u32)strlen("SH_END");
    src.data += ofs;
    src.size = sz - ofs;
    
    trunc_file(SH_SRC_OUT_URI, 0);
    write_file(SH_SRC_OUT_URI, src.data, src.size);
    
    struct os_process vp = {.p = INVALID_HANDLE_VALUE};
    struct os_process fp = {.p = INVALID_HANDLE_VALUE};
    
    char *va[] = {SH_CL_URI, "-fshader-stage=vert", SH_SRC_OUT_URI, "-Werror -std=450 -o", SH_VERT_OUT_URI, "-DVERT"};
    char *fa[] = {SH_CL_URI, "-fshader-stage=frag", SH_SRC_OUT_URI, "-Werror -std=450 -o", SH_FRAG_OUT_URI};
    
    char vcmd_buf[256];
    char fcmd_buf[256];
    struct string vcmd = flatten_pchar_array(va, (u32)cl_array_size(va), vcmd_buf, (u32)sizeof(vcmd_buf), ' ');
    struct string fcmd = flatten_pchar_array(fa, (u32)cl_array_size(fa), fcmd_buf, (u32)sizeof(vcmd_buf), ' ');
    
    int res = 0;
    
    if (os_create_process(vcmd.data, &vp)) {
        log_error("Failed to create shader compiler (vertex)");
        res = -1;
        goto out;
    }
    
    if (os_create_process(fcmd.data, &fp)) {
        log_error("Failed to create shader compiler (fragment)");
        res = -1;
        goto out;
    }
    
    int vr = os_await_process(&vp);
    int fr = os_await_process(&fp);
    
    if (vr || fr) {
        println("\nshader source dump:");
        write_stdout(src.data, src.size);
        println("\nend of shader source dump");
        log_error_if(vr, "Vertex shader compiler return non-zero error code (%i)", (s64)vr);
        log_error_if(fr, "Fragment shader compiler return non-zero error code (%i)", (s64)fr);
        res = -1;
        goto out;
    }
    
    struct string vspv,fspv;
    
    vspv.size = read_file(SH_VERT_OUT_URI, NULL, 0);
    vspv.data = salloc(MT, vspv.size);
    read_file(SH_VERT_OUT_URI, vspv.data, vspv.size);
    
    fspv.size = read_file(SH_FRAG_OUT_URI, NULL, 0);
    fspv.data = salloc(MT, fspv.size);
    read_file(SH_FRAG_OUT_URI, fspv.data, fspv.size);
    
    VkShaderModule vmod = gpu_create_shader(vspv);
    VkShaderModule fmod = gpu_create_shader(fspv);
    
    if (!vmod || !fmod) {
        log_error_if(!vmod, "Failed to create vertex shader module");
        log_error_if(!fmod, "Failed to create fragment shader module");
        if (vmod)
            vk_destroy_shmod(vmod);
        if (fmod)
            vk_destroy_shmod(fmod);
        return -1;
    }
    
    gpu->sh.vert = vmod;
    gpu->sh.frag = fmod;
    
    out:
    if (vp.p != INVALID_HANDLE_VALUE)
        os_destroy_process(&vp);
    if (fp.p != INVALID_HANDLE_VALUE)
        os_destroy_process(&fp);
    return res;
}

def_gpu_handle_win_resize(gpu_handle_win_resize)
{
    println("GPU handling resize");
    
    vkDeviceWaitIdle(gpu->dev);
    
    if (gpu_create_sc()) {
        log_error("Failed to retire old swapchain, retrying from scratch...");
        if (gpu_create_sc()) {
            log_error("Failed to create fresh swapchain");
            return -1;
        }
    }
    return 0;
}

def_gpu_update(gpu_update)
{
    if (gpu->draw.used == 0)
        return 0;
    
    gpu_inc_frame();
    gpu_await_draw_fence();
    if (cvk(gpu_sc_next_img()))
        log_error("Failed to acquire proper image from swapchain");
    gpu_reset_draw_fence();
    
    if (gpu_next_fb()) {
        log_error("Failed to create framebuffer");
        return -1;
    }
    for(u32 i=0; i < GPU_CMD_CNT; ++i) {
        vk_reset_cmdpool(gpu_cmd(i).pool, 0x0);
        gpu_dealloc_cmds(i);
    }
    
    for(u32 i=0; i < GPU_BUF_CNT; ++i) {
        if ((gpu->flags & GPU_MEM_OFS) == false) {
            gpu->buf[i].used = 0;
            gpu->buf[i].size >>= 1;
        } else {
            gpu->buf[i].used = gpu->buf[i].size;
            gpu->buf[i].size <<= 1;
        }
    }
    gpu->flags ^= GPU_MEM_OFS;
    
    gpu_db_flush();
    
    return 0;
}

def_gpu_db_add(gpu_db_add)
{
    if (gpu->draw.used == cl_array_size(gpu->draw.obj))
        return -1;
    
    return 0;
}

def_gpu_db_flush(gpu_db_flush)
{
    char msg[127];
    VkCommandBuffer cmd[GPU_CMD_CNT];
    if (gpu->flags & GPU_MEM_UNI) {
        u32 cmdi = gpu_alloc_cmds(GPU_QI_G, 1);
        if (cmdi == Max_u32) {
            log_error("Failed to allocate graphics command buffer for flushing draw buffer");
            return -1;
        }
        cmd[GPU_CI_G] = gpu_cmd(GPU_CI_G).bufs[cmdi];
        vk_begin_cmd(cmd[GPU_CI_G], GPU_CMD_OT);
    } else {
        for(u32 i=0; i < GPU_CMD_CNT; ++i) {
            u32 cmdi = gpu_alloc_cmds(i, 1);
            if (cmdi == Max_u32) {
                log_error("Failed to allocate command buffer %u (%s) for flushing draw buffer", gpu_cmd_name(i));
                return -1;
            }
            cmd[i] = gpu_cmd(GPU_CI_G).bufs[cmdi];
        }
        for(u32 i=0; i < GPU_CMD_CNT; ++i)
            vk_begin_cmd(cmd[i], GPU_CMD_OT);
    }
    
    u64 ofs;
    u64 sz = sizeof(*gpu->draw.obj) * gpu->draw.used;
    if (gpu->flags & GPU_MEM_UNI) {
        ofs = gpu_buf_alloc(GPU_BI_G, sz);
        if (ofs == Max_u64) {
            dbg_strcpy(CLSTR(msg), STR("unified memory architecture"));
            goto bufcpy_fail;
        }
        memcpy((u8*)gpu->buf[GPU_BI_G].data + ofs, gpu->draw.obj, sz);
    } else if (gpu->q[GPU_QI_T].i == gpu->q[GPU_QI_G].i) {
        dbg_strcpy(CLSTR(msg), STR("non-objscrete transfer"));
        
        ofs = gpu_buf_alloc(GPU_BI_T, sz);
        if (ofs == Max_u64) {
            goto bufcpy_fail;
        }
        
        ofs = gpu_bufcpy(cmd[GPU_CI_G], ofs, sz, GPU_BI_T, GPU_BI_G);
        if (ofs == Max_u64)
            goto bufcpy_fail;
        
        memcpy((u8*)gpu->buf[GPU_BI_T].data + ofs, gpu->draw.obj, sz);
        
        VkMemoryBarrier2 barr = {VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
        barr.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barr.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
        barr.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT;
        barr.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
        
        VkDependencyInfo dep = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        dep.memoryBarrierCount = 1;
        dep.pMemoryBarriers = &barr;
        
        vk_cmd_pl_barr(cmd[GPU_CI_G], &dep);
    } else {
        dbg_strcpy(CLSTR(msg), STR("objscrete transfer"));
        
        ofs = gpu_buf_alloc(GPU_BI_T, sz);
        if (ofs == Max_u64)
            goto bufcpy_fail;
        
        ofs = gpu_bufcpy(cmd[GPU_CI_T], ofs, sz, GPU_BI_T, GPU_BI_G);
        if (ofs == Max_u64)
            goto bufcpy_fail;
        
        memcpy((u8*)gpu->buf[GPU_BI_T].data + ofs, gpu->draw.obj, sz);
        
        VkMemoryBarrier2 barr[GPU_CMD_CNT] = {
            [GPU_CI_T] = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
            },
            [GPU_CI_G] = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT,
                .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT,
            }
        };
        
        VkDependencyInfo dep[GPU_CMD_CNT] = {
            [GPU_CI_T] = {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .memoryBarrierCount = 1,
                .pMemoryBarriers = &barr[GPU_CI_T],
            },
            [GPU_CI_G] = {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .memoryBarrierCount = 1,
                .pMemoryBarriers = &barr[GPU_CI_G],
            }
        };
        
        vk_cmd_pl_barr(cmd[GPU_CI_T], &dep[GPU_CI_T]);
        vk_cmd_pl_barr(cmd[GPU_CI_G], &dep[GPU_CI_G]);
        vk_end_cmd(cmd[GPU_CI_T]);
    }
    
    VkRect2D ra;
    ra.offset.x = 0;
    ra.offset.y = 0;
    ra.extent.width = win->dim.w;
    ra.extent.height = win->dim.h;
    
#if 0
    // NOTE(SollyCB): I would like to do this, but it can leave artifacts if I do not clear the whole screen.
    // Maybe there is a better way to account for this than clearing the entire screen?
    memset(&ra.offset, 0x7f, sizeof(ra.offset));
    memset(&ra.extent, 0x00, sizeof(ra.extent));
    for(u32 i=0; i < gpu->draw.used; ++i) {
        if (gpu->draw.obj[i].pd.ofs.x < ra.offset.x)
            ra.offset.x = gpu->draw.obj[i].pd.ofs.x;
        if (gpu->draw.obj[i].pd.ofs.y < ra.offset.y)
            ra.offset.y = gpu->draw.obj[i].pd.ofs.y;
        if (gpu->draw.obj[i].pd.ext.w + (u32)gpu->draw.obj[i].pd.ofs.x > ra.extent.width)
            ra.extent.width = gpu->draw.obj[i].pd.ext.w + gpu->draw.obj[i].pd.ofs.x;
        if (gpu->draw.obj[i].pd.ext.h + (u32)gpu->draw.obj[i].pd.ofs.y > ra.extent.height)
            ra.extent.height = gpu->draw.obj[i].pd.ext.h + gpu->draw.obj[i].pd.ofs.y;
    }
    
    // Seems to tightly fit render area to cells
    ra.offset.x = (s32)roundf((f32)ra.offset.x / 65535.0f * (f32)win->objm.w);
    ra.offset.y = (s32)roundf((f32)ra.offset.y / 65535.0f * (f32)win->objm.h);
    ra.extent.width = (s32)roundf((f32)ra.extent.width / 65535.0f * (f32)win->objm.w);
    ra.extent.height = (s32)roundf((f32)ra.extent.height / 65535.0f * (f32)win->objm.h);
    
    if (ra.offset.x > 0) ra.offset.x -= 1;
    if (ra.offset.y > 0) ra.offset.y -= 1;
    if (ra.extent.width < win->objm.w) ra.extent.width += 2;
    if (ra.extent.height < win->objm.h) ra.extent.height += 2;
    
    if (ra.extent.width > win->objm.w) ra.extent.width = win->objm.w;
    if (ra.extent.height > win->objm.h) ra.extent.height = win->objm.h;
    
#endif
    
    VkClearValue cv = {{{1.0f,1.0f,1.0f,1.0f}}};
    
    VkRenderPassBeginInfo rbi = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rbi.renderPass = gpu->rp;
    rbi.framebuffer = gpu->fb[frm_i];
    rbi.renderArea = ra;
    rbi.clearValueCount = 1;
    rbi.pClearValues = &cv;
    
    VkSubpassBeginInfo sbi = {
        .sType = VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO,
        .contents = VK_SUBPASS_CONTENTS_INLINE,
    };
    
    VkViewport vp;
    vp.x = 0.0f;
    vp.y = 0.0f;
    vp.width = (f32)win->dim.w;
    vp.height = (f32)win->dim.h;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    
    VkCommandBuffer gcmd = cmd[GPU_CI_G];
    vk_cmd_begin_rp(gcmd, &rbi, &sbi);
    vk_cmd_bind_pl(gcmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gpu->pl);
    vk_cmd_set_viewport(gcmd, 0, 1, &vp);
    vk_cmd_set_scissor(gcmd, 0, 1, &ra);
    vk_cmd_bind_ds(gcmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gpu->pll, 0, 1, &gpu->ds);
    vk_cmd_bind_vb(gcmd, 0, 1, &gpu->buf[GPU_BI_G].handle, &ofs);
    vk_cmd_draw(gcmd, 6, gpu->draw.used);
    vk_cmd_end_rp(gcmd);
    vk_end_cmd(gcmd);
    
    if ((gpu->flags & GPU_MEM_UNI) == false && gpu->q[GPU_QI_T].i != gpu->q[GPU_QI_G].i) {
        enum {WTR,WSC};
        VkSemaphore w_sem[] = {
            [WTR] = gpu->draw.sem[DB_SI_T],
            [WSC] = gpu->sc.map[gpu->sc.i].sem,
        };
        VkPipelineStageFlags w_stg[] = {
            [WTR] = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
            [WSC] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        };
        VkSubmitInfo gsi = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        gsi.waitSemaphoreCount = cl_array_size(w_sem);
        gsi.pWaitSemaphores = w_sem;
        gsi.pWaitDstStageMask = w_stg;
        gsi.commandBufferCount = 1;
        gsi.pCommandBuffers = &cmd[GPU_CI_G];
        gsi.signalSemaphoreCount = 1;
        gsi.pSignalSemaphores = &gpu->draw.sem[GPU_CI_G];
        
        VkSubmitInfo tsi = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        tsi.commandBufferCount = 1;
        tsi.pCommandBuffers = &cmd[GPU_CI_T];
        tsi.signalSemaphoreCount = 1;
        tsi.pSignalSemaphores = &gpu->draw.sem[GPU_CI_T];
        
        if (vk_qsub(gpu->q[GPU_QI_T].handle, 1, &tsi, VK_NULL_HANDLE)) {
            log_error("Failed to submit transfer commands");
            return -1;
        }
        if (vk_qsub(gpu->q[GPU_QI_G].handle, 1, &gsi, gpu->draw.fence[frm_i])) {
            log_error("Failed to submit graphics commands");
            return -1;
        }
    } else {
        enum {WSC};
        VkSemaphore w_sem[] = {
            [WSC] = gpu->sc.map[gpu->sc.i].sem,
        };
        VkPipelineStageFlags w_stg[] = {
            [WSC] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        };
        
        VkSubmitInfo gsi = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        gsi.waitSemaphoreCount = cl_array_size(w_sem);
        gsi.pWaitSemaphores = w_sem;
        gsi.pWaitDstStageMask = w_stg;
        gsi.commandBufferCount = 1;
        gsi.pCommandBuffers = &cmd[GPU_CI_G];
        gsi.signalSemaphoreCount = 1;
        gsi.pSignalSemaphores = &gpu->draw.sem[DB_SI_G];
        
        if (vk_qsub(gpu->q[GPU_QI_G].handle, 1, &gsi, gpu->draw.fence[frm_i])) {
            log_error("Failed to submit graphics commands");
            return -1;
        }
    }
    
    gpu->draw.used = 0;
    
    VkResult r;
    VkPresentInfoKHR pi = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &gpu->draw.sem[DB_SI_G];
    pi.swapchainCount = 1;
    pi.pSwapchains = &gpu->sc.handle;
    pi.pImageIndices = &gpu->sc.map[gpu->sc.i].i;
    pi.pResults = &r;
    
    if (vk_qpres(&pi)) {
        println("QueuePresent was not VK_SUCCESS, requesting window resize");
        win->flags |= WIN_RSZ;
        return -1;
    }
    
    return 0;
    
    bufcpy_fail:
    log_error("Failed to allocate gpu memory for flushing draw buffer (%s)", msg);
    return -1;
}

def_gpu_check_leaks(gpu_check_leaks)
{
    // @NOTE I am not necessarily trying to destroy everything,
    // just enough that the validation messages are parseable.
    
    vkDeviceWaitIdle(gpu->dev);
    
    u32 tmp = frm_i;
    frm_i = 0;
    for(u32 j=0; j < FRAME_WRAP; ++j) {
        for(u32 i=0; i < GPU_CMD_CNT; ++i) {
            gpu_dealloc_cmds(i);
            vk_destroy_cmdpool(gpu_cmd(i).pool);
        }
        frm_i++;
    }
    frm_i = tmp;
    
    for(u32 i=0; i < GPU_BUF_CNT; ++i) {
        if (gpu->buf[i].handle)
            vk_destroy_buf(gpu->buf[i].handle);
    }
    for(u32 i=0; i < GPU_MEM_CNT; ++i)
        vk_free_mem(gpu->mem[i].handle);
    
    vk_destroy_shmod(gpu->sh.vert);
    vk_destroy_shmod(gpu->sh.frag);
    vk_destroy_pll(gpu->pll);
    vk_destroy_pl(gpu->pl);
    vk_destroy_rp(gpu->rp);
    for(u32 i=0; i < cl_array_size(gpu->fb); ++i) {
        if (gpu->fb[i])
            vk_destroy_fb(gpu->fb[i]);
    }
    
    vk_destroy_dsl(gpu->dsl);
    vk_destroy_dp(gpu->dp);
    vk_destroy_sampler(gpu->sampler);
    
    vkDestroyDevice(gpu->dev, NULL);
}
