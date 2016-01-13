varying highp vec2 qt_TexCoord;

uniform sampler2D qt_Texture;
uniform lowp float opacity;

void main()
{
    gl_FragColor = texture2D(qt_Texture, qt_TexCoord) * opacity;
}