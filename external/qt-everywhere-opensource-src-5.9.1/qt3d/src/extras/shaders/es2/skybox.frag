varying highp vec3 texCoord0;
uniform samplerCube skyboxTexture;

void main()
{
    gl_FragColor = textureCube(skyboxTexture, texCoord0);
}

