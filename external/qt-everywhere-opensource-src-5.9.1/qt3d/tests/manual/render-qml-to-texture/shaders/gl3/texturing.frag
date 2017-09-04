#version 150 core

uniform vec3 eyePosition;

uniform sampler2D surfaceTexture;

in vec3 worldPosition;
in vec3 worldNormal;
in vec2 texCoord;

out vec4 fragColor;

void main()
{
    fragColor = texture(surfaceTexture, texCoord);
}
