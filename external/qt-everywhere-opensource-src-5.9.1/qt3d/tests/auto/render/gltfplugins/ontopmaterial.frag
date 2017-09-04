#version 150 core

uniform vec4 handleColor;
uniform float customAlpha;

uniform sampler2D customTexture;

out vec4 fragColor;

void main()
{
    vec3 customColor = texture(customTexture, vec2(0.4, 0.4)).rgb;
    fragColor = vec4(handleColor.xy, customColor.z, customAlpha);
}
