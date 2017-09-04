#version 150

uniform sampler2D source;
uniform float qt_Opacity;

in vec2 qt_TexCoord0;
in vec2 qt_TexCoord1;
in vec2 qt_TexCoord2;
in vec2 qt_TexCoord3;

out vec4 fragColor;

void main() {
    vec4 sourceColor = (texture(source, qt_TexCoord0) +
        texture(source, qt_TexCoord1) +
        texture(source, qt_TexCoord2) +
        texture(source, qt_TexCoord3)) * 0.25;
    fragColor = sourceColor * qt_Opacity;
}
