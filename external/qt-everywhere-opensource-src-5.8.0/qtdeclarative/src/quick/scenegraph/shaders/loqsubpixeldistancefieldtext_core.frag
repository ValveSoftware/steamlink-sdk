#version 150 core

in vec3 sampleNearLeft;
in vec3 sampleNearRight;

out vec4 fragColor;

uniform sampler2D _qt_texture;
uniform vec4 color;
uniform float alphaMin;
uniform float alphaMax;

void main()
{
    vec2 n;
    n.x = textureProj(_qt_texture, sampleNearLeft).r;
    n.y = textureProj(_qt_texture, sampleNearRight).r;
    n = smoothstep(alphaMin, alphaMax, n);
    float c = 0.5 * (n.x + n.y);
    fragColor = vec4(n.x, c, n.y, c) * color.w;
}