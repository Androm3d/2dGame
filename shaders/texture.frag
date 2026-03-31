#version 330

uniform vec4 color;
uniform sampler2D tex;
uniform float grayscaleAmount;
uniform float flashAmount;
uniform vec3 flashColor;

in vec2 texCoordFrag;
out vec4 outColor;

void main()
{
	// Discard fragment if texture sample has alpha < 0.5
	// otherwise compose the texture sample with the fragment's interpolated color
	vec4 texColor = texture(tex, texCoordFrag);
	if(texColor.a < 0.5f)
		discard;
	vec4 baseColor = color * texColor;
	float gray = dot(baseColor.rgb, vec3(0.299, 0.587, 0.114));
	baseColor.rgb = mix(baseColor.rgb, vec3(gray), clamp(grayscaleAmount, 0.0, 1.0));
	baseColor.rgb = mix(baseColor.rgb, flashColor, clamp(flashAmount, 0.0, 1.0));
	outColor = baseColor;
}

