#version 330 core

in EyeSpaceVertex {
    vec3 position;
    vec3 normal;
} fs_in;

out vec4 fragColor;

uniform struct LightInfo {
    vec4 position;
    vec3 intensity;
} light;

uniform struct LineInfo {
    float width;
    vec4 color;
} line;

uniform vec3 ka;            // Ambient reflectivity
uniform vec3 kd;            // Diffuse reflectivity

vec3 rimLightModel( const in vec3 pos, const in vec3 n )
{
    // Calculate the vector from the light to the fragment
    vec3 s = normalize( vec3( light.position ) - pos );

    // Calculate the vector from the fragment to the eye position (the
    // origin since this is in "eye" or "camera" space
    vec3 v = normalize( -pos );

    // Refleft the light beam using the normal at this fragment
    vec3 r = reflect( -s, n );

    // Calculate the diffuse component, which for rim lighting it 1 minus s dot n
    // rather than s dot n as for standard diffuse lighting
    float sDotN = dot( s, n );
    vec3 diffuse = vec3( 1.0 - max( sDotN, 0.0 ) );

    // Combine the ambient, diffuse and specular contributions
    return light.intensity * ( ka + kd * diffuse );
}

void main()
{
    vec3 n = gl_FrontFacing ? fs_in.normal : -fs_in.normal;
    vec4 color = vec4( rimLightModel( fs_in.position, normalize( n ) ), 1.0 );
    fragColor = color;
}
