$in vec4 v_pos;
$in vec4 v_color;
$in float v_ambientocclusion;
uniform mat4 u_viewprojection;

#include "_shadowmap.frag"

uniform mediump vec3 u_lightdir;
uniform lowp vec3 u_diffuse_color;
uniform lowp vec3 u_ambient_color;
$out vec4 o_color;

void main(void) {
#if r_renderoutline == 1
	const float epsilona = 0.025;
	const float epsilonb = 0.0001;
	float xx = fract(v_pos.x);
	float yy = fract(v_pos.y);
	float zz = fract(v_pos.z);
	bool nearX = (xx >= -epsilona && xx <= epsilona);
	bool nearY = (yy >= -epsilona && yy <= epsilona);
	bool nearZ = (zz >= -epsilona && zz <= epsilona);
	bool overX = (xx >= -epsilonb && xx <= epsilonb);
	bool overY = (yy >= -epsilonb && yy <= epsilonb);
	bool overZ = (zz >= -epsilonb && zz <= epsilonb);
	if ((nearX && !overX) || (nearY && !overY) || (nearZ && !overZ)) {
		o_color = vec4(v_color.rgb * vec3(0.3, 0.3, 0.3), 1.0);
	} else
#endif // r_renderoutline
	{
		vec3 fdx = dFdx(v_pos.xyz);
		vec3 fdy = dFdy(v_pos.xyz);
		vec3 normal = normalize(cross(fdx, fdy));
		float ndotl1 = dot(normal, u_lightdir);
		float ndotl2 = dot(normal, -u_lightdir);
		vec3 diffuse = u_diffuse_color * max(0.0, max(ndotl1, ndotl2));
		vec3 shadowColor = shadow(u_viewprojection, v_color.rgb, diffuse, u_ambient_color);
		o_color = vec4(shadowColor * v_ambientocclusion, v_color.a);
	}
}
