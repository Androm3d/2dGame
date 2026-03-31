#version 330

uniform vec4 color;
uniform sampler2D tex;
uniform float grayscaleAmount;
uniform float flashAmount;
uniform vec3 flashColor;
uniform float warpAmount;
uniform float warpPhase;
uniform float time;
uniform float godModeAmount;

in vec2 texCoordFrag;
out vec4 outColor;

void main()
{
	// Discard fragment if texture sample has alpha < 0.5
	// otherwise compose the texture sample with the fragment's interpolated color
	vec2 uv = texCoordFrag;
	if (warpAmount > 0.0001)
	{
		vec2 centerDelta = uv - vec2(0.5, 0.5);
		float r = length(centerDelta);
		if (r < 0.75)
		{
			vec2 dir = (r > 0.0001) ? (centerDelta / r) : vec2(0.0, 1.0);
			float ripple = sin(20.0 * r - warpPhase * 10.0) * 0.012 * warpAmount;
			float swirl = (0.75 - r) * 0.018 * warpAmount;
			uv += dir * ripple + vec2(-dir.y, dir.x) * swirl;
		}
	}
	vec4 texColor = texture(tex, uv);
	if(texColor.a < 0.5f)
		discard;
	vec4 baseColor = color * texColor;
	float gray = dot(baseColor.rgb, vec3(0.299, 0.587, 0.114));
	baseColor.rgb = mix(baseColor.rgb, vec3(gray), clamp(grayscaleAmount, 0.0, 1.0));
	baseColor.rgb = mix(baseColor.rgb, flashColor, clamp(flashAmount, 0.0, 1.0));
	if (godModeAmount > 0.001)
	{
		float gm = clamp(godModeAmount, 0.0, 1.0);
		vec3 rainbow = 0.5 + 0.5 * sin(vec3(0.0, 2.0943951, 4.1887902) + time * 5.5);
		float pulse = 0.5 + 0.5 * sin(time * 10.0);
		float centerGlow = 1.0 - clamp(distance(uv, vec2(0.5, 0.5)) / 0.78, 0.0, 1.0);
		vec3 glowTint = rainbow * (0.7 + 0.6 * pulse);
		vec3 boosted = baseColor.rgb * (1.15 + 0.35 * pulse) + glowTint * (0.35 + 0.45 * centerGlow);
		baseColor.rgb = mix(baseColor.rgb, boosted, gm);
	}
	baseColor.rgb = clamp(baseColor.rgb, 0.0, 1.0);
	outColor = baseColor;
}

