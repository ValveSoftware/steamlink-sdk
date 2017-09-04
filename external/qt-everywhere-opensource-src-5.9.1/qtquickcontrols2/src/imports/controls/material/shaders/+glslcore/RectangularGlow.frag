#version 150

uniform float qt_Opacity;
uniform float relativeSizeX;
uniform float relativeSizeY;
uniform float spread;
uniform vec4 color;

in vec2 qt_TexCoord0;
out vec4 fragColor;

float linearstep(float e0, float e1, float x)
{
    return clamp((x - e0) / (e1 - e0), 0.0, 1.0);
}

void main()
{
    float alpha =
        smoothstep(0.0, relativeSizeX, 0.5 - abs(0.5 - qt_TexCoord0.x)) *
        smoothstep(0.0, relativeSizeY, 0.5 - abs(0.5 - qt_TexCoord0.y));

    float spreadMultiplier = linearstep(spread, 1.0 - spread, alpha);
    fragColor = color * qt_Opacity * spreadMultiplier * spreadMultiplier;
}
