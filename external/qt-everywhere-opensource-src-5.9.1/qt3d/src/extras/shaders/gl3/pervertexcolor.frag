#version 150 core

in vec3 worldPosition;
in vec3 worldNormal;
in vec3 color;

out vec4 fragColor;

#pragma include light.inc.frag

void main()
{
    vec3 diffuseColor;
    adModel(worldPosition, worldNormal, diffuseColor);
    fragColor = vec4( color + color * diffuseColor, 1.0 );
}
