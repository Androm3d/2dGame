#version 330

in vec4 particleColor;
out vec4 outColor;

void main()
{
	vec2 p = gl_PointCoord * 2.0 - 1.0;
	float dist2 = dot(p, p);
	if (dist2 > 1.0)
		discard;
	float alpha = (1.0 - dist2) * particleColor.a;
	outColor = vec4(particleColor.rgb, alpha);
}
