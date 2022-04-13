#version 330

layout(location = 0) in vec2 pos;		// Position in clip-space
layout(location = 1) in vec2 uv;		// Texture coordinate

smooth out vec2 fragUV;		// Interpolated texture coordinate

uniform mat4 xform;			// Aspect ratio preservation matrix
uniform int viewmode;

void main() {
	// Transform vertex position
	gl_Position = vec4(pos, 0.0, 1.0);

	// Interpolate texture coordinates
	if (viewmode == 0)
		fragUV = uv;
	else
		fragUV = vec2(xform * vec4(uv, 0.0, 1.0));
}