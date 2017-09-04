#version 150 core

in vec2 qt_TexCoord;

out vec4 fragColor;

uniform sampler2D qt_Texture;
uniform float opacity;

void main()
{
    fragColor = texture(qt_Texture, qt_TexCoord) * opacity;
}