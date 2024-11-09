#define SH_CL_URI "C:/VulkanSDK/1.3.296.0/Bin/glslc.exe"

#define SH_SRC_URI "../shader.h"
#define SH_SRC_OUT_URI "shader.glsl"
#define SH_VERT_OUT_URI "shader.vert.spv"
#define SH_FRAG_OUT_URI "shader.frag.spv"

#define SH_ENTRY_POINT "main"

#define SH_BEGIN

#define SH_COL_LOC 0
#define SH_POS_LOC 1

#if GL_core_profile /* search token for gpu_compile_sh */

#extension GL_EXT_debug_printf : require
#extension GL_EXT_nonuniform_qualifier : require

struct vf_info_t {
    vec4 col;
};

void pv4(vec4 v)
{
    debugPrintfEXT("(%f, %f, %f, %f)\n", v.x, v.y, v.z, v.w);
}

#ifdef VERT
/****************************************************/
// Vertex shader

// I would like to parse the spirv to get these automatically
// but I don't think that the VkFormats would be correct
// because I want to pass in normalized r8/r16, not float...
layout(location = SH_POS_LOC) in vec2 pos;
layout(location = SH_COL_LOC) in vec4 col;

layout(location = 0) out vf_info_t vf_info;

void main() {
    gl_PointSize = 1;
    gl_Position.xy = (pos - 0.5) * 2;
    gl_Position.zw = vec2(0,1);
    vf_info.col = col;
}
#else
/****************************************************/
// Fragment shader

layout(location = 0) in vf_info_t vf_info;
layout(location = 0) out vec4 fc;

void main() {
    fc = vf_info.col;
}
#endif // shader switch

#endif // #if SHADER_SRC_GUARD

#define SH_END /* search token for gpu_compile_sh */

/* Leave whitespace following SH_END to ensure file is valid */