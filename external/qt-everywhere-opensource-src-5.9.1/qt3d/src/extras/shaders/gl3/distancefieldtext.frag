#version 150 core

uniform sampler2D distanceFieldTexture;
uniform float minAlpha;
uniform float maxAlpha;
uniform float textureSize;
uniform vec4 color;

in vec3 position;
in vec2 texCoord;

out vec4 fragColor;

void main()
{
    // determine the scale of the glyph texture within pixel-space coordinates
    // (that is, how many pixels are drawn for each texel)
    vec2 texelDeltaX = abs(dFdx(texCoord));
    vec2 texelDeltaY = abs(dFdy(texCoord));
    float avgTexelDelta = textureSize * 0.5 * (texelDeltaX.x + texelDeltaX.y + texelDeltaY.x + texelDeltaY.y);
    float texScale = 1.0 / avgTexelDelta;

    // scaled to interval [0.0, 0.15]
    float devScaleMin = 0.00;
    float devScaleMax = 0.15;
    float scaled = (clamp(texScale, devScaleMin, devScaleMax) - devScaleMin) / (devScaleMax - devScaleMin);

    // thickness of glyphs should increase a lot for very small glyphs to make them readable
    float base = 0.5;
    float threshold = base * scaled;
    float range = 0.06 / texScale;

    float minAlpha = threshold - range;
    float maxAlpha = threshold + range;

    float distVal = texture(distanceFieldTexture, texCoord).r;
    fragColor = color * smoothstep(minAlpha, maxAlpha, distVal);
}
