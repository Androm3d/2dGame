#version 330

in vec2 position;
out vec2 fragCoord;
uniform mat4 projection;

void main()
{
    fragCoord = position;
    gl_Position = projection * vec4(position, 0.0, 1.0);
}