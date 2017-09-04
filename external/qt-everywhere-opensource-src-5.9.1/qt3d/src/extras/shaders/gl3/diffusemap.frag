#version 150 core

uniform vec3 ka;            // Ambient reflectivity
uniform vec3 ks;            // Specular reflectivity
uniform float shininess;    // Specular shininess factor

uniform vec3 eyePosition;

uniform sampler2D diffuseTexture;

in vec3 worldPosition;
in vec3 worldNormal;
in vec2 texCoord;

out vec4 fragColor;

#pragma include light.inc.frag

void main()
{
    vec3 diffuseTextureColor = texture( diffuseTexture, texCoord ).rgb;

    vec3 diffuseColor, specularColor;
    adsModel(worldPosition, worldNormal, eyePosition, shininess, diffuseColor, specularColor);

    fragColor = vec4( diffuseTextureColor * ( ka + diffuseColor ) + ks * specularColor, 1.0 );
}
