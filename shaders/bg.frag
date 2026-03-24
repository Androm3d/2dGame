#version 330

in vec2 fragCoord;
out vec4 outColor;
uniform float time; 

void main()
{
    vec2 uv = normalize(fragCoord);
    
    float cloudSpeed = time * 0.2;
    float colorMix = 0.5 + 0.5 * sin(cloudSpeed + uv.x * 5.0 + uv.y * 3.0);
    vec3 color1 = vec3(0.1, 0.4, 0.8);
    vec3 color2 = vec3(0.6, 0.8, 1.0); 
    
    outColor = vec4(mix(color1, color2, colorMix), 1.0);
}