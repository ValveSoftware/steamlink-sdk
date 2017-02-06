#define FP highp

uniform sampler2D diffuseTexture;

varying FP vec3 position;
varying FP vec2 texCoord;

void main()
{
    gl_FragColor = texture2D( diffuseTexture, texCoord );
}
