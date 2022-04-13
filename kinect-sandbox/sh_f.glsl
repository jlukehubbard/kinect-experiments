#version 330

smooth in vec2 fragUV;	// Interpolated texture coordinate

uniform sampler2D tex;	// Texture sampler
uniform int viewmode;	// Color or depth viewing

out vec3 outCol;	// Final pixel color

void main() {
	// Sample texture as output color
	if (viewmode == 0)
		outCol = texture2D(tex, fragUV).rgb;
	else
		outCol = vec3(1.0) - texture2D(tex, fragUV).rrr;
}