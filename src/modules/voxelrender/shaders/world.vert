// attributes from the VAOs
$in vec3 a_pos;
$in uvec3 a_info;

#ifdef INSTANCED
// instanced rendering
$in vec3 a_offset;
#endif // INSTANCED

#ifndef MATERIALOFFSET
#define MATERIALOFFSET 0
#endif // MATERIALOFFSET
int materialoffset = MATERIALOFFSET;

uniform mat4 u_model;
uniform mat4 u_viewprojection;
uniform sampler2D u_texture;
#define MATERIALCOLORS 256
layout(std140) uniform u_materialblock {
	vec4 u_materialcolor[MATERIALCOLORS];
};

$out vec3 v_pos;
$out vec4 v_color;
$out float v_ambientocclusion;

#include "_fog.vert"
#include "_shadowmap.vert"
#include "_ambientocclusion.vert"

void main(void) {
	uint a_ao = a_info[0];
	uint a_colorindex = a_info[1];
	uint a_material = a_info[2];
#ifdef INSTANCED
	vec4 pos = vec4(a_offset, 0.0) + u_model * vec4(a_pos, 1.0);
#else // INSTANCED
	vec4 pos = u_model * vec4(a_pos, 1.0);
#endif // INSTANCED
	v_pos = pos.xyz;

	int materialColorIndex = int(a_colorindex) + materialoffset;
	vec3 materialColor = u_materialcolor[materialColorIndex % MATERIALCOLORS].rgb;
	vec3 colornoise = texture(u_texture, abs(pos.xz) / 256.0 / 10.0).rgb;
	float alpha = u_materialcolor[a_colorindex].a;
	// TODO: use $constant to check this magic number with the code
	if (a_material == 1u) {
		alpha = 0.6;
	}
	v_color = clamp(vec4(materialColor * colornoise * 1.8, alpha), 0.0, 1.0);

	v_ambientocclusion = aovalues[a_ao];

#if cl_shadowmap == 1
	v_lightspacepos = v_pos;
	v_viewz = (u_viewprojection * vec4(v_lightspacepos, 1.0)).w;
#endif // cl_shadowmap

	gl_Position = u_viewprojection * pos;
}
