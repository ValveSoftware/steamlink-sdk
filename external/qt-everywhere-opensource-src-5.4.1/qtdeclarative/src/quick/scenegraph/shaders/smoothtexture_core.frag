#version 150 core

in vec2 texCoord;
in float vertexOpacity;

out vec4 fragColor;

uniform sampler2D qt_Texture;

void main()
{
    fragColor = texture(qt_Texture, texCoord) * vertexOpacity;
}