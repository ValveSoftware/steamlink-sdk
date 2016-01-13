#version 150

in vec2 qt_TexCoord0;

out vec4 fragColor;

uniform sampler2D source;
uniform float qt_Opacity;

void main()
{
    fragColor = texture(source, qt_TexCoord0) * qt_Opacity;
}