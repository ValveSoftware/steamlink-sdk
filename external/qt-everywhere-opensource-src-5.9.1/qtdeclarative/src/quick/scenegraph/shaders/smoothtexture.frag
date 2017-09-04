uniform sampler2D qt_Texture;

varying highp vec2 texCoord;
varying lowp float vertexOpacity;

void main()
{
    gl_FragColor = texture2D(qt_Texture, texCoord) * vertexOpacity;
}