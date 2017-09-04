#version 150 core

uniform vec4 lightPosition = vec4(1.0, 1.0, 0.0, 1.0);
uniform vec3 lightIntensity = vec3(1.0, 1.0, 1.0);

uniform vec3 ka;            // Ambient reflectivity
uniform vec3 ks = vec3(1.0, 1.0, 1.0);            // Specular reflectivity
uniform float shininess = 150.0;    // Specular shininess factor

in vec3 kd;            // Diffuse reflectivity
in vec3 position;
in vec3 normal;

out vec4 fragColor;

vec3 adsModel( const in vec3 pos, const in vec3 n )
{
    // Calculate the vector from the light to the fragment
    vec3 s = normalize( vec3( lightPosition ) - pos );

    // Calculate the vector from the fragment to the eye position
    // (origin since this is in "eye" or "camera" space)
    vec3 v = normalize( -pos );

    // Reflect the light beam using the normal at this fragment
    vec3 r = reflect( -s, n );

    // Calculate the diffuse component
    float diffuse = max( dot( s, n ), 0.0 );

    // Calculate the specular component
    float specular = 0.0;
    if ( dot( s, n ) > 0.0 )
        specular = pow( max( dot( r, v ), 0.0 ), shininess );

    // Combine the ambient, diffuse and specular contributions
    return lightIntensity * ( ka + kd * diffuse + ks * specular );
}

void main()
{
    fragColor = vec4( adsModel( position, normalize( normal ) ), 1.0 );
}
