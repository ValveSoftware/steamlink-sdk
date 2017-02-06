#version 120

varying highp vec3 pos;
varying highp vec3 rayDir;

uniform highp sampler3D textureSampler;
uniform highp vec4 colorIndex[256];
uniform highp int color8Bit;
uniform highp vec3 textureDimensions;
uniform highp int sampleCount; // This is the maximum sample count
uniform highp float alphaMultiplier;
uniform highp int preserveOpacity;
uniform highp vec3 minBounds;
uniform highp vec3 maxBounds;

// Ray traveling straight through a single 'alpha thickness' applies 100% of the encountered alpha.
// Rays traveling shorter distances apply a fraction. This is used to normalize the alpha over
// entire volume, regardless of texture dimensions
const highp float alphaThicknesses = 32.0;
const highp float SQRT3 = 1.73205081;

void main() {
    vec3 rayStart = pos;
    highp vec3 startBounds = minBounds;
    highp vec3 endBounds = maxBounds;
    if (rayDir.x < 0.0) {
        startBounds.x = maxBounds.x;
        endBounds.x = minBounds.x;
    }
    if (rayDir.y > 0.0) {
        startBounds.y = maxBounds.y;
        endBounds.y = minBounds.y;
    }
    if (rayDir.z > 0.0) {
        startBounds.z = maxBounds.z;
        endBounds.z = minBounds.z;
    }

    // Calculate ray intersection endpoint
    highp vec3 rayStop;
    highp vec3 invRayDir = 1.0 / rayDir;
    highp vec3 t = (endBounds - rayStart) * invRayDir;
    highp float endT = min(t.x, min(t.y, t.z));
    if (endT <= 0.0)
        discard;
    rayStop = rayStart + endT * rayDir;

    // Convert intersections to texture coords
    rayStart = 0.5 * (rayStart + 1.0);
    rayStop = 0.5 * (rayStop + 1.0);

    highp vec3 ray = rayStop - rayStart;

    highp float fullDist = length(ray);
    highp float stepSize = SQRT3 / sampleCount;
    highp vec3 step = (SQRT3 * normalize(ray)) / sampleCount;

    rayStart += (step * 0.001);

    highp vec3 curPos = rayStart;
    highp float curLen = 0.0;
    highp vec4 curColor = vec4(0, 0, 0, 0);
    highp float curAlpha = 0.0;
    highp vec3 curRgb = vec3(0, 0, 0);
    highp vec4 destColor = vec4(0, 0, 0, 0);
    highp float totalOpacity = 1.0;

    highp float extraAlphaMultiplier = stepSize * alphaThicknesses * alphaMultiplier;

    // Raytrace into volume, need to sample pixels along the eye ray until we hit opacity 1
    for (int i = 0; i < sampleCount; i++) {
        curColor = texture3D(textureSampler, curPos);
        if (color8Bit != 0)
            curColor = colorIndex[int(curColor.r * 255.0)];

        if (curColor.a >= 0.0) {
            if (curColor.a == 1.0 && (preserveOpacity == 1 || alphaMultiplier >= 1.0))
                curAlpha = 1.0;
            else
                curAlpha = curColor.a * extraAlphaMultiplier;
            highp float nextOpacity = totalOpacity - curAlpha;
            // If opacity goes beyond full opacity, we need to adjust current alpha according
            // to the fraction of the distance the material is visible, so that we don't get
            // box artifacts around texels.
            if (nextOpacity < 0.0) {
                curAlpha *= totalOpacity / curAlpha;
                nextOpacity = 0.0;
            }

            curRgb = curColor.rgb * curAlpha * (totalOpacity + nextOpacity);
            destColor.rgb += curRgb;
            totalOpacity = nextOpacity;
        }
        curPos += step;
        curLen += stepSize;
        if (curLen >= fullDist || totalOpacity <= 0.0)
            break;
    }

    if (totalOpacity == 1.0)
        discard;

    // Brighten up the final color if there is some transparency left
    if (totalOpacity >= 0.0 && totalOpacity < 1.0)
        destColor *= (1.0 - (totalOpacity * 0.5)) / (1.0 - totalOpacity);

    destColor.a = (1.0 - totalOpacity);
    gl_FragColor = clamp(destColor, 0.0, 1.0);
}
