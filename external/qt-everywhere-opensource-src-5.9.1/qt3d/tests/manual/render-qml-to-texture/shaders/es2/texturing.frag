#define FP highp

uniform FP vec3 eyePosition;

uniform sampler2D surfaceTexture;

varying FP vec3 worldPosition;
varying FP vec3 worldNormal;
varying FP vec2 texCoord;



void main()
{
    gl_FragColor = texture2D(surfaceTexture, texCoord);
}
