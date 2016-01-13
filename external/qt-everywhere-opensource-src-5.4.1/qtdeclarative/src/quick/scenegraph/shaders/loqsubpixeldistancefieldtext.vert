uniform highp mat4 matrix;
uniform highp vec2 textureScale;
uniform highp float fontScale;
uniform highp vec4 vecDelta;

attribute highp vec4 vCoord;
attribute highp vec2 tCoord;

varying highp vec3 sampleNearLeft;
varying highp vec3 sampleNearRight;

void main()
{
    highp vec2 sampleCoord = tCoord * textureScale;
    gl_Position = matrix * vCoord;

    // Calculate neighbor pixel position in item space.
    highp vec3 wDelta = gl_Position.w * vecDelta.xyw;
    highp vec3 nearLeft = vCoord.xyw - 0.25 * wDelta;
    highp vec3 nearRight = vCoord.xyw + 0.25 * wDelta;

    // Calculate neighbor texture coordinate.
    highp vec2 scale = textureScale / fontScale;
    highp vec2 base = sampleCoord - scale * vCoord.xy;
    sampleNearLeft = vec3(base * nearLeft.z + scale * nearLeft.xy, nearLeft.z);
    sampleNearRight = vec3(base * nearRight.z + scale * nearRight.xy, nearRight.z);
}