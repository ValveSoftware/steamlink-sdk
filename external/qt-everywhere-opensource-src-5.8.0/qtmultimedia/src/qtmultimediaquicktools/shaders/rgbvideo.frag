uniform sampler2D rgbTexture;
uniform lowp float opacity;
varying highp vec2 qt_TexCoord;

void main()
{
    gl_FragColor = texture2D(rgbTexture, qt_TexCoord) * opacity;
}
