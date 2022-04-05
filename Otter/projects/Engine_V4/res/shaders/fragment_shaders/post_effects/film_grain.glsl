#version 430

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec3 outColor;

uniform layout(binding = 0) sampler2D s_Image;

uniform float u_Filter[9];
uniform vec2 u_PixelSize;

uniform float u_time;

const float noiseStrength = 30.0;

void main() {
    outColor = texture(s_Image, inUV).xyz;

    float x = (inUV.x + 4) * (inUV.y + 4) * (u_time * 10);
   vec3 grain = vec3(mod((mod(x, 13) + 1) * (mod(x, 123) + 1), 0.01) - 0.005) * noiseStrength;

    outColor += grain;
}