#version 150 core

in vec3 worldPosition;
in vec2 texCoord;
in vec2 waveTexCoord;
in vec2 movtexCoord;
in vec2 multexCoord;
in vec2 skyTexCoord;

in vec3 vpos;

in mat3 tangentMatrix;
in vec3 color;

uniform sampler2D diffuseTexture;
uniform sampler2D specularTexture;
uniform sampler2D normalTexture;
uniform sampler2D waveTexture;
uniform sampler2D skyTexture;
uniform sampler2D foamTexture;

uniform float offsetx;
uniform float offsety;
uniform float specularity;
uniform float waveStrenght;
uniform vec3 ka;
uniform vec3 specularColor;
uniform float shininess;
uniform float normalAmount;
uniform vec3 eyePosition;

out vec4 fragColor;

#pragma include light.inc.frag

void main()
{
    // Move waveTexCoords
    vec2 waveMovCoord = waveTexCoord;
    waveMovCoord.x += offsetx;
    waveMovCoord.y -= offsety;
    vec4 wave = texture(waveTexture, waveMovCoord);

    //Wiggle the newCoord by r and b colors of waveTexture
    vec2 newCoord = texCoord;
    newCoord.x += wave.r * waveStrenght;
    newCoord.y -= wave.b * waveStrenght;

    // Sample the textures at the interpolated texCoords
    // Use default texCoord for diffuse (it does not move on x or y, so it can be used as "ground under the water").
    vec4 diffuseTextureColor = texture(diffuseTexture, texCoord);
    // 2 Animated Layers of specularTexture mixed with the newCoord
    vec4 specularTextureColor = texture( specularTexture, multexCoord+newCoord) + (texture( specularTexture, movtexCoord+newCoord ));
    // 2 Animated Layers of normalTexture mixed with the newCoord
    vec3 normal = normalAmount * texture( normalTexture, movtexCoord+newCoord ).rgb - vec3( 1.0 )+(normalAmount * texture( normalTexture, multexCoord+newCoord ).rgb - vec3( 1.0 ));
    // Animated skyTexture layer
    vec4 skycolor = texture(skyTexture, skyTexCoord);
    skycolor = skycolor * 0.4;
    //Animated foamTexture layer
    vec4 foamTextureColor = texture(foamTexture, texCoord);

    // Calculate the lighting model, keeping the specular component separate
    vec3 diffuseColor, specularColor;
    adsModelNormalMapped(worldPosition, normal, eyePosition, shininess, tangentMatrix, diffuseColor, specularColor);

    // Combine final fragment color
    vec4 outputColor = vec4(((skycolor.rgb + ka + diffuseTextureColor.rgb * (diffuseColor))+(specularColor * specularTextureColor.a*specularity)), vpos.y );


    outputColor += (foamTextureColor.rgba*vpos.y);

    fragColor = vec4(outputColor.rgb,1.0);
}

