#define FP highp

varying FP vec3 worldPosition;
varying FP vec3 worldNormal;
varying FP vec3 color;

#pragma include light.inc.frag

void main()
{
    FP vec3 diffuseColor;
    adModel(worldPosition, worldNormal, diffuseColor);
    gl_FragColor = vec4( color + color * diffuseColor, 1.0 );
}
