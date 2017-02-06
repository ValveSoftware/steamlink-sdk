#version 120

varying highp vec3 pos;
varying highp vec3 rayDir;

uniform highp sampler3D textureSampler;
uniform highp vec3 volumeSliceIndices;
uniform highp vec4 colorIndex[256];
uniform highp int color8Bit;
uniform highp float alphaMultiplier;
uniform highp int preserveOpacity;
uniform highp vec3 minBounds;
uniform highp vec3 maxBounds;

const highp vec3 xPlaneNormal = vec3(1.0, 0, 0);
const highp vec3 yPlaneNormal = vec3(0, 1.0, 0);
const highp vec3 zPlaneNormal = vec3(0, 0, 1.0);

void main() {
    // Find out where ray intersects the slice planes
    vec3 normRayDir = normalize(rayDir);
    highp vec3 rayStart = pos;
    highp float minT = 2.0;
    if (normRayDir.x != 0.0 && normRayDir.y != 0.0 && normRayDir.z != 0.0) {
        highp vec3 boxBounds = vec3(1.0, 1.0, 1.0);
        highp vec3 invRayDir = 1.0 / normRayDir;
        if (normRayDir.x < 0)
            boxBounds.x = -1.0;
        if (normRayDir.y < 0)
            boxBounds.y = -1.0;
        if (normRayDir.z < 0)
            boxBounds.z = -1.0;
        highp vec3 t = (boxBounds - rayStart) * invRayDir;
        minT = min(t.x, min(t.y, t.z));
    }

    highp vec3 xPoint = vec3(volumeSliceIndices.x, 0, 0);
    highp vec3 yPoint = vec3(0, volumeSliceIndices.y, 0);
    highp vec3 zPoint = vec3(0, 0, volumeSliceIndices.z);
    highp float firstD = minT + 1.0;
    highp float secondD = firstD;
    highp float thirdD = firstD;
    if (volumeSliceIndices.x >= -1.0) {
        highp float dx = dot(xPoint - rayStart, xPlaneNormal) / dot(normRayDir, xPlaneNormal);
        if (dx >= 0.0 && dx <= minT)
            firstD = min(dx, firstD);
    }
    if (volumeSliceIndices.y >= -1.0) {
        highp float dy = dot(yPoint - rayStart, yPlaneNormal) / dot(normRayDir, yPlaneNormal);
        if (dy >= 0.0 && dy <= minT) {
            if (dy < firstD) {
                secondD = firstD;
                firstD = dy;
            } else {
                secondD = dy;
            }
        }
    }
    if (volumeSliceIndices.z >= -1.0) {
        highp float dz = dot(zPoint - rayStart, zPlaneNormal) / dot(normRayDir, zPlaneNormal);
        if (dz >= 0.0) {
            if (dz < firstD && dz <= minT) {
                thirdD = secondD;
                secondD = firstD;
                firstD = dz;
            } else if (dz < secondD){
                thirdD = secondD;
                secondD = dz;
            } else {
                thirdD = dz;
            }
        }
    }

    highp vec4 destColor = vec4(0.0, 0.0, 0.0, 0.0);
    highp vec4 curColor = vec4(0.0, 0.0, 0.0, 0.0);
    highp float totalAlpha = 0.0;
    highp vec3 curRgb = vec3(0, 0, 0);
    highp float curAlpha = 0.0;

    // Convert intersection to texture coords

    if (firstD <= minT) {
        highp vec3 texelVec = rayStart + normRayDir * firstD;
        if (clamp(texelVec.x, minBounds.x, maxBounds.x) == texelVec.x
                && clamp(texelVec.y, maxBounds.y, minBounds.y) == texelVec.y
                && clamp(texelVec.z, maxBounds.z, minBounds.z) == texelVec.z) {
            texelVec = 0.5 * (texelVec + 1.0);
            curColor = texture3D(textureSampler, texelVec);
            if (color8Bit != 0)
                curColor = colorIndex[int(curColor.r * 255.0)];

            if (curColor.a > 0.0) {
                curAlpha = curColor.a;
                if (curColor.a == 1.0 && preserveOpacity != 0)
                    curAlpha = 1.0;
                else
                    curAlpha = clamp(curColor.a * alphaMultiplier, 0.0, 1.0);
                destColor.rgb = curColor.rgb * curAlpha;
                totalAlpha = curAlpha;
            }
        }
        if (secondD <= minT && totalAlpha < 1.0) {
            texelVec = rayStart + normRayDir * secondD;
            if (clamp(texelVec.x, minBounds.x, maxBounds.x) == texelVec.x
                    && clamp(texelVec.y, maxBounds.y, minBounds.y) == texelVec.y
                    && clamp(texelVec.z, maxBounds.z, minBounds.z) == texelVec.z) {
                texelVec = 0.5 * (texelVec + 1.0);
                curColor = texture3D(textureSampler, texelVec);
                if (color8Bit != 0)
                    curColor = colorIndex[int(curColor.r * 255.0)];
                if (curColor.a > 0.0) {
                    if (curColor.a == 1.0 && preserveOpacity != 0)
                        curAlpha = 1.0;
                    else
                        curAlpha = clamp(curColor.a * alphaMultiplier, 0.0, 1.0);
                    curRgb = curColor.rgb * curAlpha * (1.0 - totalAlpha);
                    destColor.rgb += curRgb;
                    totalAlpha += curAlpha;
                }
            }
            if (thirdD <= minT && totalAlpha < 1.0) {
                texelVec = rayStart + normRayDir * thirdD;
                if (clamp(texelVec.x, minBounds.x, maxBounds.x) == texelVec.x
                        && clamp(texelVec.y, maxBounds.y, minBounds.y) == texelVec.y
                        && clamp(texelVec.z, maxBounds.z, minBounds.z) == texelVec.z) {
                    texelVec = 0.5 * (texelVec + 1.0);
                    curColor = texture3D(textureSampler, texelVec);
                    if (curColor.a > 0.0) {
                        if (color8Bit != 0)
                            curColor = colorIndex[int(curColor.r * 255.0)];
                        if (curColor.a == 1.0 && preserveOpacity != 0)
                            curAlpha = 1.0;
                        else
                            curAlpha = clamp(curColor.a * alphaMultiplier, 0.0, 1.0);
                        curRgb = curColor.rgb * curAlpha * (1.0 - totalAlpha);
                        destColor.rgb += curRgb;
                        totalAlpha += curAlpha;
                    }
                }
            }
        }
    }

    if (totalAlpha == 0.0)
        discard;

    // Brighten up the final color if there is some transparency left
    if (totalAlpha > 0.0 && totalAlpha < 1.0)
        destColor *= 1.0 / totalAlpha;

    destColor.a = totalAlpha;
    gl_FragColor = clamp(destColor, 0.0, 1.0);
}

