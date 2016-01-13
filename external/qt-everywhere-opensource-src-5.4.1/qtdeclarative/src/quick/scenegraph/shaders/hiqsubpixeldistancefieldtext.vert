uniform highp mat4 matrix;
uniform highp vec2 textureScale;
uniform highp float fontScale;
uniform highp vec4 vecDelta;

attribute highp vec4 vCoord;
attribute highp vec2 tCoord;

varying highp vec2 sampleCoord;
varying highp vec3 sampleFarLeft;
varying highp vec3 sampleNearLeft;
varying highp vec3 sampleNearRight;
varying highp vec3 sampleFarRight;

void main()
{
    sampleCoord = tCoord * textureScale;
    gl_Position = matrix * vCoord;

    // Calculate neighbor pixel position in item space.
    highp vec3 wDelta = gl_Position.w * vecDelta.xyw;
    highp vec3 farLeft = vCoord.xyw - 0.667 * wDelta;
    highp vec3 nearLeft = vCoord.xyw - 0.333 * wDelta;
    highp vec3 nearRight = vCoord.xyw + 0.333 * wDelta;
    highp vec3 farRight = vCoord.xyw + 0.667 * wDelta;

    // Calculate neighbor texture coordinate.
    highp vec2 scale = textureScale / fontScale;
    highp vec2 base = sampleCoord - scale * vCoord.xy;
    sampleFarLeft = vec3(base * farLeft.z + scale * farLeft.xy, farLeft.z);
    sampleNearLeft = vec3(base * nearLeft.z + scale * nearLeft.xy, nearLeft.z);
    sampleNearRight = vec3(base * nearRight.z + scale * nearRight.xy, nearRight.z);
    sampleFarRight = vec3(base * farRight.z + scale * farRight.xy, farRight.z);
}