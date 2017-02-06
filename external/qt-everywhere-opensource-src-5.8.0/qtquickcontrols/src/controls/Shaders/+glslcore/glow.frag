#version 150

uniform sampler2D source1;
uniform sampler2D source2;
uniform sampler2D source3;
uniform sampler2D source4;
uniform sampler2D source5;
uniform float weight1;
uniform float weight2;
uniform float weight3;
uniform float weight4;
uniform float weight5;
uniform vec4 color;
uniform float spread;
uniform float qt_Opacity;

in vec2 qt_TexCoord0;

out vec4 fragColor;

float linearstep(float e0, float e1, float x) {
    return clamp((x - e0) / (e1 - e0), 0.0, 1.0);
}

void main() {
    vec4 sourceColor = texture(source1, qt_TexCoord0) * weight1;
    sourceColor += texture(source2, qt_TexCoord0) * weight2;
    sourceColor += texture(source3, qt_TexCoord0) * weight3;
    sourceColor += texture(source4, qt_TexCoord0) * weight4;
    sourceColor += texture(source5, qt_TexCoord0) * weight5;
    sourceColor = mix(vec4(0), color, linearstep(0.0, spread, sourceColor.a));
    fragColor = sourceColor * qt_Opacity;
}
