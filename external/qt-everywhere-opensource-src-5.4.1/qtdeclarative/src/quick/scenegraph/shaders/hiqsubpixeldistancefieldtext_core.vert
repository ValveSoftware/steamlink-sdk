#version 150 core

in vec4 vCoord;
in vec2 tCoord;

out vec2 sampleCoord;
out vec3 sampleFarLeft;
out vec3 sampleNearLeft;
out vec3 sampleNearRight;
out vec3 sampleFarRight;

uniform mat4 matrix;
uniform vec2 textureScale;
uniform float fontScale;
uniform vec4 vecDelta;

void main()
{
    sampleCoord = tCoord * textureScale;
    gl_Position = matrix * vCoord;

    // Calculate neighbor pixel position in item space.
    vec3 wDelta = gl_Position.w * vecDelta.xyw;
    vec3 farLeft = vCoord.xyw - 0.667 * wDelta;
    vec3 nearLeft = vCoord.xyw - 0.333 * wDelta;
    vec3 nearRight = vCoord.xyw + 0.333 * wDelta;
    vec3 farRight = vCoord.xyw + 0.667 * wDelta;

    // Calculate neighbor texture coordinate.
    vec2 scale = textureScale / fontScale;
    vec2 base = sampleCoord - scale * vCoord.xy;
    sampleFarLeft = vec3(base * farLeft.z + scale * farLeft.xy, farLeft.z);
    sampleNearLeft = vec3(base * nearLeft.z + scale * nearLeft.xy, nearLeft.z);
    sampleNearRight = vec3(base * nearRight.z + scale * nearRight.xy, nearRight.z);
    sampleFarRight = vec3(base * farRight.z + scale * farRight.xy, farRight.z);
}