#version 330

uniform mat4 projection, modelview;
uniform vec2 texCoordDispl;
uniform float time;
uniform float mapSwayAmount;

in vec2 position;
in vec2 texCoord;
out vec2 texCoordFrag;

void main()
{
	// Pass texture coordinates
	texCoordFrag = texCoord + texCoordDispl;

	// Optional map wind sway for specific foliage tiles only.
	// IDs 97 and 105 are tilesheet col=0,row=12 and col=0,row=13 in an 8x17 atlas.
	vec2 pos = position;
	int col = int(floor(texCoord.x * 8.0));
	int row = int(floor(texCoord.y * 17.0));
	if(mapSwayAmount > 0.001 && col == 0 && (row == 12 || row == 13))
	{
		float phase = time * 2.3 + position.x * 0.03 + position.y * 0.015;
		float swayPx = sin(phase) * 2.2 * mapSwayAmount;
		pos.x += swayPx;
	}

	// Transform position from pixel coordinates to clipping coordinates
	gl_Position = projection * modelview * vec4(pos, 0.0, 1.0);
}

