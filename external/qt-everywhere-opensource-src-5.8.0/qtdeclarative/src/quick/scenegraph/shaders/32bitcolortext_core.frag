#version 150 core

in vec2 sampleCoord;

out vec4 fragColor;

uniform sampler2D _qt_texture;
uniform float color; // just the alpha, really...

void main()
{
    fragColor = texture(_qt_texture, sampleCoord) * color;
}
