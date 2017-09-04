#version 150 core

in vec3 vertexPosition;
in vec3 vertexNormal;
in vec3 pos;

out vec3 position;
out vec3 normal;
out vec3 kd;

uniform mat4 modelView;
uniform mat3 modelViewNormal;
uniform mat4 mvp;

// Calculate RGB triplet from HSV
vec3 hsvToRGB( float h, float s, float v )
{
    if ( s <= 0.0 )
        return vec3( v );

    h = h * 6.0;
    float c = v * s;
    float x = ( 1.0 - abs( ( mod( h, 2 ) - 1 ) ) ) * c;
    float m = v - c;
    float r = 0.0;
    float g = 0.0;
    float b = 0.0;

    if ( h < 1.0 )      { r = c;   g = x;   b = 0.0;}
    else if ( h < 2.0 ) { r = x;   g = c;   b = 0.0; }
    else if ( h < 3.0 ) { r = 0.0; g = c;   b = x; }
    else if ( h < 4.0 ) { r = 0.0; g = x;   b = c; }
    else if ( h < 5.0 ) { r = x;   g = 0.0; b = c; }
    else                { r = c;   g = 0.0; b = x; }

    return vec3( r + m, g + m, b + m );
}

void main()
{
    normal = normalize( modelViewNormal * vertexNormal );

    vec3 offsetPos = vertexPosition + pos;
    position = vec3(modelView * vec4(offsetPos, 1.0));

    float hue = fract( pos.x / 10.0 );
    float saturation = fract( pos.z / 20.0 );
    kd = hsvToRGB( hue, saturation, 1.0 );

    gl_Position = mvp * vec4(offsetPos, 1.0);
}
