#version 330

uniform mat4 projection;
uniform mat4 modelview;

in vec2 position;
in vec4 inColor;
in float inSize;

out vec4 particleColor;

void main()
{
	particleColor = inColor;
	gl_Position = projection * modelview * vec4(position, 0.0, 1.0);
	gl_PointSize = inSize;
}
