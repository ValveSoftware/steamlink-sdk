#define FP highp

uniform FP sampler2D distanceFieldTexture;
uniform FP float minAlpha;
uniform FP float maxAlpha;
uniform FP float textureSize;
uniform FP vec4 color;

varying FP vec3 position;
varying FP vec2 texCoord;

void main()
{
    // determine the scale of the glyph texture within pixel-space coordinates
    // (that is, how many pixels are drawn for each texel)
    FP vec2 texelDeltaX = abs(dFdx(texCoord));
    FP vec2 texelDeltaY = abs(dFdy(texCoord));
    FP float avgTexelDelta = textureSize * 0.5 * (texelDeltaX.x + texelDeltaX.y + texelDeltaY.x + texelDeltaY.y);
    FP float texScale = 1.0 / avgTexelDelta;

    // scaled to interval [0.0, 0.15]
    FP float devScaleMin = 0.00;
    FP float devScaleMax = 0.15;
    FP float scaled = (clamp(texScale, devScaleMin, devScaleMax) - devScaleMin) / (devScaleMax - devScaleMin);

    // thickness of glyphs should increase a lot for very small glyphs to make them readable
    FP float base = 0.5;
    FP float threshold = base * scaled;
    FP float range = 0.06 / texScale;

    FP float minAlpha = threshold - range;
    FP float maxAlpha = threshold + range;

    FP float distVal = texture2D(distanceFieldTexture, texCoord).r;
    gl_FragColor = color * smoothstep(minAlpha, maxAlpha, distVal);
}
