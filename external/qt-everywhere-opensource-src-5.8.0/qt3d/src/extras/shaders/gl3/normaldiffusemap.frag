#version 150 core

in vec3 worldPosition;
in vec2 texCoord;
in mat3 tangentMatrix;

uniform sampler2D diffuseTexture;
uniform sampler2D normalTexture;

// TODO: Replace with a struct
uniform vec3 ka;            // Ambient reflectivity
uniform vec3 ks;            // Specular reflectivity
uniform float shininess;    // Specular shininess factor

uniform vec3 eyePosition;

out vec4 fragColor;

#pragma include light.inc.frag

void main()
{
    // Sample the textures at the interpolated texCoords
    vec4 diffuseTextureColor = texture( diffuseTexture, texCoord );
    vec3 normal = 2.0 * texture( normalTexture, texCoord ).rgb - vec3( 1.0 );

    // Calculate the lighting model, keeping the specular component separate
    vec3 diffuseColor, specularColor;
    adsModelNormalMapped(worldPosition, normal, eyePosition, shininess, tangentMatrix, diffuseColor, specularColor);

    // Combine spec with ambient+diffuse for final fragment color
    fragColor = vec4( ka + diffuseTextureColor.rgb * diffuseColor + ks * specularColor, 1.0 );
}
