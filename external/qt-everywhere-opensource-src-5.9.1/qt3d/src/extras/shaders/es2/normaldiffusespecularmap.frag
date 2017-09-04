#define FP highp

varying FP vec3 worldPosition;
varying FP vec2 texCoord;
varying FP mat3 tangentMatrix;

uniform sampler2D diffuseTexture;
uniform sampler2D specularTexture;
uniform sampler2D normalTexture;

// TODO: Replace with a struct
uniform FP vec3 ka;            // Ambient reflectivity
uniform FP float shininess;    // Specular shininess factor

uniform FP vec3 eyePosition;

#pragma include light.inc.frag

void main()
{
    // Sample the textures at the interpolated texCoords
    FP vec4 diffuseTextureColor = texture2D( diffuseTexture, texCoord );
    FP vec4 specularTextureColor = texture2D( specularTexture, texCoord );
    FP vec3 normal = 2.0 * texture2D( normalTexture, texCoord ).rgb - vec3( 1.0 );

    // Calculate the lighting model, keeping the specular component separate
    FP vec3 diffuseColor, specularColor;
    adsModelNormalMapped(worldPosition, normal, eyePosition, shininess, tangentMatrix, diffuseColor, specularColor);

    // Combine spec with ambient+diffuse for final fragment color
    gl_FragColor = vec4( ka + diffuseTextureColor.rgb * diffuseColor + specularTextureColor.rgb * specularColor, 1.0 );
}
