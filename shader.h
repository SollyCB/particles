#define SH_CL_URI "C:/VulkanSDK/1.3.296.0/Bin/glslc.exe"

#define SH_SRC_URI "../shader.h"
#define SH_SRC_OUT_URI "shader.glsl"
#define SH_VERT_OUT_URI "shader.vert.spv"
#define SH_FRAG_OUT_URI "shader.frag.spv"

#define SH_ENTRY_POINT "main"

#define SH_BEGIN

#define SH_SI_CNT (126 - 33 + 1) /* 126 == tilde, 33 == exclamation mark */
#define SH_SI_SET 0 /* sampler descriptor set index */
#define SH_SI_BND 0 /* sampler descriptor set binding */

#define SH_COL_LOC 0
#define SH_POS_LOC 1

#if GL_core_profile /* search token for gpu_compile_sh */

#extension GL_EXT_debug_printf : require
#extension GL_EXT_nonuniform_qualifier : require

struct vf_info_t {
    vec4 fg;
    vec4 bg;
    vec2 tc;
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
layout(location = SH_POS_LOC) in vec3 pos;
layout(location = SH_COL_LOC) in vec4 col;

layout(location = 0) out vf_info_t vf_info;

void main() {
    gl_Position.xyz = pos;
    
    vf_info.fg = fg;
    vf_info.bg = bg;
    vf_info.tc = offset[index[gl_VertexIndex]] * 0.5;
}
#else
/****************************************************/
// Fragment shader

layout(set = SH_SI_SET, binding = SH_SI_BND) uniform sampler2D glyph[SH_SI_CNT];

layout(location = 0) in vf_info_t vf_info;

layout(location = 0) out vec4 fc;

void main() {
    uint c = uint(vf_info.bg.a);
    vec3 bg = vf_info.bg.xyz / 255;
    
    float g = texture(glyph[nonuniformEXT(c)], vf_info.tc).r * vf_info.fg.a;
    vec3 col = mix(bg, vf_info.fg.rgb, g);
    fc = vec4(col, 1);
}
#endif // shader switch

#endif // #if SHADER_SRC_GUARD

#define SH_END /* search token for gpu_compile_sh */

/* Leave whitespace following SH_END to ensure file is valid */