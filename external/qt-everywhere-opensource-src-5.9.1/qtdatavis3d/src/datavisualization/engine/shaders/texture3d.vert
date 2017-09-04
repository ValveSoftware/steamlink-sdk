uniform highp mat4 MVP;
uniform highp vec3 minBounds;
uniform highp vec3 maxBounds;
uniform highp vec3 cameraPositionRelativeToModel;

attribute highp vec3 vertexPosition_mdl;
attribute highp vec2 vertexUV;
attribute highp vec3 vertexNormal_mdl;

varying highp vec3 pos;
varying highp vec3 rayDir;

void main() {
    gl_Position = MVP * vec4(vertexPosition_mdl, 1.0);

    highp vec3 minBoundsNorm = minBounds;
    highp vec3 maxBoundsNorm = maxBounds;

    // Y and Z are flipped in bounds to be directly usable in texture calculations,
    // so flip them back to normal for position calculations
    minBoundsNorm.yz = -minBoundsNorm.yz;
    maxBoundsNorm.yz = -maxBoundsNorm.yz;

    minBoundsNorm = 0.5 * (minBoundsNorm + 1.0);
    maxBoundsNorm = 0.5 * (maxBoundsNorm + 1.0);

    pos = vertexPosition_mdl
            + ((1.0 - vertexPosition_mdl) * minBoundsNorm)
            - ((1.0 + vertexPosition_mdl) * (1.0 - maxBoundsNorm));

    rayDir = -(cameraPositionRelativeToModel - pos);

    // Flip Y and Z so QImage bits work directly for texture and first image is in the front
    rayDir.yz = -rayDir.yz;
    pos.yz = -pos.yz;
}
