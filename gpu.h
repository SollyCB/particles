#ifndef GPU_H
#define GPU_H

#include <vulkan/vulkan_core.h>

#include "shader.h"

#define SC_MAX_IMGS 4 /* Arbitrarily small size that I doubt will be exceeded */
#define SC_MIN_IMGS 2
#define FRAME_WRAP 2

extern u32 frm_i; // frame index, is either 0 or 1

enum {
    DB_SI_T, // transfer complete
    DB_SI_G, // color output complete
    DB_SEM_CNT,
};

#define GPU_MAX_CMDS 16

enum gpu_mem_indices {
    GPU_MI_V,
    GPU_MI_T,
    GPU_MEM_CNT,
};

enum gpu_buf_indices {
    GPU_BI_V,
    GPU_BI_T,
    GPU_BUF_CNT,
};

enum gpu_q_indices {
    GPU_QI_G,
    GPU_QI_T,
    GPU_QI_P,
    GPU_Q_CNT,
};

// represents queues that can be submitted to
enum gpu_cmdq_indices {
    GPU_CI_G,
    GPU_CI_T,
    GPU_CMD_CNT
};

struct gpu {
    VkInstance inst;
    VkSurfaceKHR surf;
    VkPhysicalDeviceProperties props;
    VkPhysicalDeviceMemoryProperties memprops;
    VkPhysicalDevice phys_dev;
    VkDevice dev;
    
    u32 flags;
    u32 q_cnt;
    
    struct {
        VkQueue handle;
        u32 i;
        struct {
            u32 buf_cnt;
            VkCommandPool pool;
            VkCommandBuffer bufs[GPU_MAX_CMDS];
        } cmd[FRAME_WRAP];
    } q[GPU_Q_CNT];
    
    VkDeviceMemory mem[GPU_MEM_CNT];
    
    struct {
        VkBuffer handle;
        void *data;
        u64 size;
        u64 used;
    } buf[GPU_BUF_CNT];
    
    u32 buffer_size; // true buffer size is double this, but buffers are split in half per frame
    
    struct {
        VkSwapchainKHR handle;
        VkSwapchainCreateInfoKHR info;
        
        struct {
            VkImage img;
            VkImageView view;
        } att[SC_MAX_IMGS];
        
        u32 img_cnt,i;
        
        struct {
            u32 i;
            VkSemaphore sem;
        } map[SC_MAX_IMGS];
    } sc;
    
    struct {
        VkShaderModule vert;
        VkShaderModule frag;
    } sh;
    
    VkPipelineLayout pll;
    VkPipeline pl;
    VkRenderPass rp;
    VkFramebuffer fb[FRAME_WRAP];
    
    VkDescriptorSetLayout dsl;
    VkDescriptorPool dp;
    VkDescriptorSet ds;
    
    struct {
        struct {
            struct rgba col;
            struct offset_u16 pos;
        } *elem; // pointer into transfer/vertex buffer
        
        u32 used;
        VkFence fence[FRAME_WRAP];
        
        VkSemaphore sem[FRAME_WRAP][GPU_BUF_CNT];
        
    } draw;
};

#ifdef LIB
extern struct gpu *gpu;

#define def_create_gpu(name) int name(void)
def_create_gpu(create_gpu);

#define def_gpu_create_sh(name) int name(void)
def_gpu_create_sh(gpu_create_sh);

#define def_gpu_handle_win_resize(name) int name(void)
def_gpu_handle_win_resize(gpu_handle_win_resize);

#define def_gpu_add_draw_elem(name) int name(struct rgba col, struct offset_u16 pos)
def_gpu_add_draw_elem(gpu_add_draw_elem);

#define def_gpu_draw(name) int name(void)
def_gpu_draw(gpu_draw);

#define def_gpu_update(name) int name(void)
def_gpu_update(gpu_update);

#define def_gpu_check_leaks(name) void name(void)
def_gpu_check_leaks(gpu_check_leaks);

/**********************************************************************/
// gpu.c and vdt.c helper stuff

#define GPU_DS 0
#define GPU_DS_CNT 10

#define sc_att gpu->sc.att[gpu->sc.map[gpu->sc.i].i]

// TODO(SollyCB): I would REALLY like to have a way to define typesafe
// integers...

enum gpu_cmd_opt {
    GPU_CMD_OT = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    GPU_CMD_RE = VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT,
};

union gpu_memreq_info {
    VkBufferCreateInfo *buf;
    VkImageCreateInfo *img;
};

enum cell_vertex_fmts {
    CELL_POS_FMT = VK_FORMAT_R16G16_UNORM,
    CELL_COL_FMT = VK_FORMAT_R8G8B8A8_UNORM,
};

enum gpu_fb_attachment_indices {
    GPU_FB_AI_SWAP, // swapchain image resolve
};

extern u32 gpu_ci_to_qi[GPU_CMD_CNT];
extern char* gpu_mem_names[GPU_MEM_CNT];
extern char *gpu_cmdq_names[GPU_CMD_CNT];

#define gpu_que(qi) gpu->q[qi]
#define gpu_mem(mi) gpu->mem[mi]
#define gpu_buf(bi) gpu->buf[bi]
#define gpu_buf_name(bi) gpu_mem_names[bi]
#define gpu_cmd_name(ci) gpu_cmdq_names[ci]
#define gpu_cmd(ci) gpu->q[gpu_ci_to_qi[ci]].cmd[frm_i]

#define gpu_sc_map gpu->sc.map[gpu->sc.i]
#define gpu_sc_sem gpu->sc.map[gpu->sc.i].sem
#define gpu_draw_sem(bi) gpu->draw.sem[frm_i][bi]

#endif // ifdef LIB

#endif
