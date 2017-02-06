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
    rayStop = rayStart + endT * rayDir;
    if (endT <= 0.0)
        discard;

    // Convert intersections to texture coords
    rayStart = 0.5 * (rayStart + 1.0);
    rayStop = 0.5 * (rayStop + 1.0);

    highp vec3 ray = rayStop - rayStart;

    highp vec3 absRay = abs(ray);
    highp vec3 invAbsRay = 1.0 / absRay;
    highp float fullDist = length(ray);
    highp vec3 curPos = rayStart;

    highp vec4 curColor = vec4(0, 0, 0, 0);
    highp float curAlpha = 0.0;
    highp float curLen = 0.0;
    highp vec3 curRgb = vec3(0, 0, 0);

    highp vec4 destColor = vec4(0, 0, 0, 0);
    highp float totalOpacity = 1.0;

    highp float extraAlphaMultiplier = fullDist * alphaThicknesses * alphaMultiplier;

    // nextEdges vector indicates the next edges of the texel boundaries along each axis that
    // the ray is about to cross. The first edges are offset by a fraction of a texel to
    // avoid artifacts from rounding errors later.
    highp vec3 nextEdges = vec3(floor(curPos.x / textureDimensions.x) * textureDimensions.x,
                                floor(curPos.y / textureDimensions.y) * textureDimensions.y,
                                floor(curPos.z / textureDimensions.z) * textureDimensions.z);

    highp vec3 textureSteps = textureDimensions;
    highp vec3 textureOffset = textureDimensions * 0.001;
    if (ray.x > 0) {
        nextEdges.x += textureDimensions.x + textureOffset.x;
    } else {
        nextEdges.x -= textureOffset.x;
        textureSteps.x = -textureDimensions.x;
    }
    if (ray.y > 0) {
        nextEdges.y += textureDimensions.y + textureOffset.y;
    } else {
        nextEdges.y -= textureOffset.y;
        textureSteps.y = -textureDimensions.y;
    }
    if (ray.z > 0) {
        nextEdges.z += textureDimensions.z + textureOffset.z;
    } else {
        nextEdges.z -= textureOffset.z;
        textureSteps.z = -textureDimensions.z;
    }

    // Raytrace into volume, need to sample pixels along the eye ray until we hit opacity 1
    for (int i = 0; i < sampleCount; i++) {
        curColor = texture3D(textureSampler, curPos);
        if (color8Bit != 0)
            curColor = colorIndex[int(curColor.r * 255.0)];

        // Find which dimension has least to go to figure out the next step distance
        highp vec3 delta = abs(nextEdges - curPos);
        highp vec3 modDelta = delta * invAbsRay;
        highp float minDelta = min(modDelta.x, min(modDelta.y, modDelta.z));
        highp float stepSize;
        if (minDelta == modDelta.x) {
            nextEdges.x += textureSteps.x;
            stepSize = delta.x * invAbsRay.x;
        }
        if (minDelta == modDelta.y) {
            nextEdges.y += textureSteps.y;
            stepSize = delta.y * invAbsRay.y;
        }
        if (minDelta == modDelta.z) {
            nextEdges.z += textureSteps.z;
            stepSize = delta.z * invAbsRay.z;
        }

        curPos += stepSize * ray;
        curLen += stepSize;

        if (curColor.a >= 0.0) {
            if (curColor.a == 1.0 && (preserveOpacity == 1 || alphaMultiplier >= 1.0))
                curAlpha = 1.0;
            else
                curAlpha = curColor.a * extraAlphaMultiplier * stepSize;
            highp float nextOpacity = totalOpacity - curAlpha;
            // If opacity goes beyond full opacity, we need to adjust current alpha according
            // to the fraction of the distance the material is visible, so that we don't get
            // box artifacts around texels.
            if (nextOpacity < 0.0) {
                curAlpha *= totalOpacity / curAlpha;
                nextOpacity = 0.0;
            }
            curRgb = curColor.rgb * curAlpha * (totalOpacity + nextOpacity);
            totalOpacity = nextOpacity;
            destColor.rgb += curRgb;
        }

        if (curLen >= 1.0 || totalOpacity <= 0.0)
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
