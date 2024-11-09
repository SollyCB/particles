#include "prg.h"
#include "win.h"
#include "gpu.h"
#include "shader.h"
#include "world.h"

struct gpu *gpu;

u32 frm_i = 0;
static inline void gpu_inc_frame(void)
{
    frm_i = (frm_i + 1) % FRAME_WRAP;
}

char* gpu_mem_names[GPU_MEM_CNT] = {
    [GPU_MI_V] = "Vertex",
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

internal int gpu_create_draw_objs(void)
{
    gpu->buffer_size = sizeof(*gpu->draw.elem) * win->max.w * win->max.h;
    
    if (gpu->props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        VkBufferCreateInfo ci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        ci.size = gpu->buffer_size * FRAME_WRAP;
        ci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        
        if (vk_create_buf(&ci, &gpu_buf(GPU_BI_V).handle)) {
            log_error("Failed to create vertex buffer");
            return -1;
        }
        
        u32 req = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        
        VkMemoryRequirements mr;
        vk_get_buf_memreq(gpu_buf(GPU_BI_V).handle, &mr);
        
        VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = mr.size;
        ai.memoryTypeIndex = gpu_memtype_helper(mr.memoryTypeBits, req);
        
        if (vk_alloc_mem(&ai, &gpu_mem(GPU_MI_V))) {
            log_error("Failed to allocate vertex buffer memory");
            return -1;
        }
        if (vk_bind_buf_mem(gpu_buf(GPU_BI_V).handle, gpu_mem(GPU_MI_V), 0)) {
            log_error("Failed to bind vertex memory");
            return -1;
        }
        
        VkBufferCreateInfo tci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        tci.size = gpu->buffer_size * FRAME_WRAP;
        tci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        
        if (vk_create_buf(&tci, &gpu_buf(GPU_BI_T).handle)) {
            log_error("Failed to create transfer buffer");
            return -1;
        }
        
        u32 treq = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        
        VkMemoryRequirements tmr;
        vk_get_buf_memreq(gpu_buf(GPU_BI_T).handle, &tmr);
        
        VkMemoryAllocateInfo tai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        tai.allocationSize = tmr.size;
        tai.memoryTypeIndex = gpu_memtype_helper(tmr.memoryTypeBits, treq);
        
        if (vk_alloc_mem(&tai, &gpu_mem(GPU_MI_T))) {
            log_error("Failed to allocate transfer buffer memory");
            return -1;
        }
        if (vk_map_mem(gpu_mem(GPU_MI_T), 0, tmr.size, &gpu_buf(GPU_BI_T).data)) {
            log_error("Failed to map transfer mem");
            return -1;
        }
        if (vk_bind_buf_mem(gpu_buf(GPU_BI_T).handle, gpu_mem(GPU_MI_T), 0)) {
            log_error("Failed to bind transfer memory");
            return -1;
        }
        gpu->draw.elem = gpu_buf(GPU_BI_T).data;
    } else {
        VkBufferCreateInfo ci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        ci.size = gpu->buffer_size * FRAME_WRAP;
        ci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        
        if (vk_create_buf(&ci, &gpu_buf(GPU_BI_V).handle)) {
            log_error("Failed to create vertex buffer");
            return -1;
        }
        
        u32 req = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT|VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        
        VkMemoryRequirements mr;
        vk_get_buf_memreq(gpu_buf(GPU_BI_V).handle, &mr);
        
        VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = mr.size;
        ai.memoryTypeIndex = gpu_memtype_helper(mr.memoryTypeBits, req);
        
        if (vk_alloc_mem(&ai, &gpu_mem(GPU_MI_V))) {
            log_error("Failed to allocate vertex buffer memory");
            return -1;
        }
        if (vk_map_mem(gpu_mem(GPU_MI_V), 0, mr.size, &gpu_buf(GPU_BI_V).data)) {
            log_error("Failed to map vertex mem");
            return -1;
        }
        if (vk_bind_buf_mem(gpu_buf(GPU_BI_V).handle, gpu_mem(GPU_MI_V), 0)) {
            log_error("Failed to bind vertex memory");
            return -1;
        }
        gpu->draw.elem = gpu_buf(GPU_BI_V).data;
    }
    
    for(u32 i=0; i < cl_array_size(gpu->draw.fence); ++i)
        vk_create_fence(true, &gpu->draw.fence[i]);
    
    for(u32 i=0; i < cl_array_size(gpu->draw.sem); ++i) {
        for(u32 j=0; j < cl_array_size(gpu->draw.sem[i]); ++j)
            vk_create_sem(&gpu->draw.sem[i][j]);
    }
    
    for(u32 i=0; i < cl_array_size(gpu_que(GPU_QI_G).cmd); ++i) {
        VkCommandPoolCreateInfo ci = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        ci.queueFamilyIndex = gpu_que(GPU_QI_G).i;
        if (vk_create_cmdpool(&ci, &gpu_que(GPU_QI_G).cmd[i].pool)) {
            log_error("Failed to create graphics command pool");
            return -1;
        }
    }
    
    if (gpu->props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        for(u32 i=0; i < cl_array_size(gpu_que(GPU_QI_T).cmd); ++i) {
            VkCommandPoolCreateInfo ci = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
            ci.queueFamilyIndex = gpu_que(GPU_QI_T).i;
            if (vk_create_cmdpool(&ci, &gpu_que(GPU_QI_T).cmd[i].pool)) {
                log_error("Failed to create graphics command pool");
                return -1;
            }
        }
    }
    
    return 0;
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
#if GPU_DS
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
#endif
    
    return 0;
}

internal int gpu_create_pll(void)
{
    VkPipelineLayoutCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
#if GPU_DS
        .setLayoutCount = 1,
        .pSetLayouts = &gpu->dsl,
#endif
    };
    
    if (vk_create_pll(&ci, &gpu->pll))
        return -1;
    
    return 0;
}

internal int gpu_create_ds(void)
{
#if GPU_DS
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
    
    VkImageView a[] = {
        sc_att.view,
    };
    
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
        .stride = sizeof(*gpu->draw.elem),
        .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE,
    };
    
    local_persist VkVertexInputAttributeDescription vi_a[] = {
        [SH_COL_LOC] = {
            .location = SH_COL_LOC,
            .format = CELL_COL_FMT,
            .offset = offsetof(typeof(*gpu->draw.elem), col),
        },
        [SH_POS_LOC] = {
            .location = SH_POS_LOC,
            .format = CELL_POS_FMT,
            .offset = offsetof(typeof(*gpu->draw.elem), pos),
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
        .topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
    };
    
    local_persist VkViewport view = {
        .x = 0,
        .y = 0,
        .width = 640, // set dynamically
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
    
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    
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
        
        vk_get_phys_dev_memprops(gpu->phys_dev, &gpu->memprops);
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
    gpu_create_draw_objs();
    
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

def_gpu_add_draw_elem(gpu_add_draw_elem)
{
    if (gpu->draw.used >= (u32)win->max.w * win->max.h) {
        log_error("Gpu draw buffer overflow");
        return -1;
    }
    
    gpu->draw.elem[gpu->draw.used].col = col;
    gpu->draw.elem[gpu->draw.used].pos = pos;
    ++gpu->draw.used;
    
    return 0;
}

def_gpu_draw(gpu_draw)
{
    VkCommandBuffer cmd;
    {
        u32 i = gpu_alloc_cmds(GPU_CI_G, 1);
        if (i == Max_u32) {
            log_error("Failed to allocate graphics command buffer");
            return -1;
        }
        cmd = gpu_cmd(GPU_CI_G).bufs[i];
        
        vk_begin_cmd(cmd, true);
    }
    
    VkBufferCopy reg;
    reg.srcOffset = gpu->buffer_size * frm_i;
    reg.dstOffset = gpu->buffer_size * frm_i;
    reg.size = gpu->draw.used * sizeof(*gpu->draw.elem);
    
    if (gpu->props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        if (gpu_que(GPU_QI_G).i == gpu_que(GPU_QI_T).i) {
            vk_cmd_bufcpy(cmd, 1, &reg, gpu_buf(GPU_BI_T).handle, gpu_buf(GPU_BI_V).handle);
            
            VkMemoryBarrier2 b = {VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
            b.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            b.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT_KHR;
            b.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT;
            b.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
            
            VkDependencyInfo dep = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
            dep.memoryBarrierCount = 1;
            dep.pMemoryBarriers = &b;
            
            vk_cmd_pl_barr(cmd, &dep);
        } else {
            VkCommandBuffer tcmd;
            {
                u32 i = gpu_alloc_cmds(GPU_CI_T, 1);
                if (i == Max_u32) {
                    log_error("Failed to allocate transfer command buffer");
                    return -1;
                }
                tcmd = gpu_cmd(GPU_CI_T).bufs[i];
                vk_begin_cmd(tcmd, true);
            }
            
            vk_cmd_bufcpy(tcmd, 1, &reg, gpu_buf(GPU_BI_T).handle, gpu_buf(GPU_BI_V).handle);
            
            VkBufferMemoryBarrier2 b = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
            b.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR;
            b.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT_KHR;
            b.srcQueueFamilyIndex = gpu_que(GPU_QI_T).i;
            b.dstQueueFamilyIndex = gpu_que(GPU_QI_G).i;
            b.buffer = gpu_buf(GPU_BI_V).handle;
            b.offset = gpu->buffer_size * frm_i;
            b.size = gpu->buffer_size;
            
            VkDependencyInfoKHR dep = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
            dep.bufferMemoryBarrierCount = 1;
            dep.pBufferMemoryBarriers = &b;
            
            vk_cmd_pl_barr(tcmd, &dep);
            
            vk_end_cmd(tcmd);
            
            b.srcStageMask = 0x0,
            b.srcAccessMask = 0x0,
            b.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT_KHR,
            b.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR,
            
            vk_cmd_pl_barr(cmd, &dep);
            
            VkSubmitInfo tsi = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
            tsi.commandBufferCount = 1;
            tsi.pCommandBuffers = &tcmd;
            tsi.signalSemaphoreCount = 1;
            tsi.pSignalSemaphores = &gpu_draw_sem(GPU_BI_T);
            
            if (vk_qsub(gpu_que(GPU_QI_T).handle, 1, &tsi, VK_NULL_HANDLE)) {
                log_error("Failed to submit transfer commands");
                return -1;
            }
        }
    }
    
    u64 ofs = 0;
    
    VkViewport vp = {};
    vp.width = win->dim.w;
    vp.height = win->dim.h;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    
    VkRect2D sc = {};
    sc.extent.width = win->dim.w;
    sc.extent.height = win->dim.h;
    
    VkClearValue cv = {};
    
    VkRenderPassBeginInfo rbi = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rbi.renderPass = gpu->rp;
    rbi.framebuffer = gpu->fb[frm_i];
    rbi.renderArea = sc;
    rbi.clearValueCount = 1;
    rbi.pClearValues = &cv;
    
    VkSubpassBeginInfo sbi = {VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO};
    sbi.contents = VK_SUBPASS_CONTENTS_INLINE;
    
    vk_cmd_set_viewport(cmd, 0, 1, &vp);
    vk_cmd_set_scissor(cmd, 0, 1, &sc);
    vk_cmd_bind_pl(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gpu->pl);
    vk_cmd_bind_vb(cmd, 0, 1, &gpu_buf(GPU_BI_V).handle, &ofs);
    
    vk_cmd_begin_rp(cmd, &rbi, &sbi);
    vk_cmd_draw(cmd, 1, gpu->draw.used);
    vk_cmd_end_rp(cmd);
    
    vk_end_cmd(cmd);
    
    if (gpu->props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        
        VkSemaphore w_sem[] = {gpu_draw_sem(GPU_BI_T), gpu_sc_sem};
        VkPipelineStageFlags w_stg[] = {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        
        VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.waitSemaphoreCount = 2;
        si.pWaitSemaphores = w_sem;
        si.pWaitDstStageMask = w_stg;
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cmd;
        si.signalSemaphoreCount = 1;
        si.pSignalSemaphores = &gpu_draw_sem(GPU_BI_V);
        
        if (vk_qsub(gpu_que(GPU_QI_G).handle, 1, &si, VK_NULL_HANDLE)) {
            log_error("Failed to submit graphics commands");
            return -1;
        }
    } else {
        VkPipelineStageFlags w_stg = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        
        VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.waitSemaphoreCount = 1;
        si.pWaitSemaphores = &gpu_sc_sem;
        si.pWaitDstStageMask = &w_stg;
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cmd;
        si.signalSemaphoreCount = 1;
        si.pSignalSemaphores = &gpu_draw_sem(GPU_BI_V);
        
        if (vk_qsub(gpu_que(GPU_QI_G).handle, 1, &si, gpu->draw.fence[frm_i])) {
            log_error("Failed to submit graphics commands");
            return -1;
        }
    }
    
    VkPresentInfoKHR pi = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &gpu_draw_sem(GPU_BI_V);
    pi.swapchainCount = 1;
    pi.pSwapchains = &gpu->sc.handle;
    pi.pImageIndices = &gpu_sc_map.i;
    pi.pResults = VK_NULL_HANDLE;
    
    if (vk_qpres(&pi)) {
        log_error("Failed to present to swapchain");
        return -1;
    }
    
    return 0;
}

def_gpu_update(gpu_update)
{
    gpu_add_draw_elem(RGBA(255, 0, 0, 255), OFFSET_U16(32000,32000));
    
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
        if (gpu->props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU && i == GPU_CI_T)
            continue;
        vk_reset_cmdpool(gpu_cmd(i).pool, 0x0);
        gpu_dealloc_cmds(i);
    }
    
    gpu_draw();
    
    return 0;
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
        vk_free_mem(gpu->mem[i]);
    
    vk_destroy_shmod(gpu->sh.vert);
    vk_destroy_shmod(gpu->sh.frag);
    vk_destroy_pll(gpu->pll);
    vk_destroy_pl(gpu->pl);
    vk_destroy_rp(gpu->rp);
    
    for(u32 i=0; i < cl_array_size(gpu->fb); ++i) {
        if (gpu->fb[i])
            vk_destroy_fb(gpu->fb[i]);
    }
    
    for(u32 i=0; i < cl_array_size(gpu->sc.map); ++i) {
        if (gpu->sc.map[i].sem) vk_destroy_sem(gpu->sc.map[i].sem);
    }
    for(u32 i=0; i < gpu->sc.img_cnt; ++i) {
        if (gpu->sc.att[i].view) vk_destroy_imgv(gpu->sc.att[i].view);
    }
    vk_destroy_sc_khr(gpu->sc.handle);
    
    vk_destroy_dsl(gpu->dsl);
    vk_destroy_dp(gpu->dp);
    
    for(u32 i=0; i < cl_array_size(gpu->draw.fence); ++i)
        vk_destroy_fence(gpu->draw.fence[i]);
    for(u32 i=0; i < cl_array_size(gpu->draw.sem); ++i) {
        for(u32 j=0; j < cl_array_size(gpu->draw.sem[i]); ++j)
            vk_destroy_sem(gpu->draw.sem[i][j]);
    }
    
    vkDestroyDevice(gpu->dev, NULL);
    
    while(1) {}
}
