uniform sampler2D rgbTexture;
uniform lowp float opacity;
varying highp vec2 qt_TexCoord;

void main()
{
    gl_FragColor =  vec4(texture2D(rgbTexture, qt_TexCoord).bgr, 1.0) * opacity;
}
