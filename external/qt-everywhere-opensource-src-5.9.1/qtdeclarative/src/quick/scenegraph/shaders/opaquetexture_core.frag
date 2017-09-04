#version 150 core

in vec2 qt_TexCoord;

out vec4 fragColor;

uniform sampler2D qt_Texture;

void main()
{
    fragColor = texture(qt_Texture, qt_TexCoord);
}