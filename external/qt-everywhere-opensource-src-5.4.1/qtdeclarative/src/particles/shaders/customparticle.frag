#version 120

varying highp vec2 qt_TexCoord0;

uniform sampler2D source;
uniform lowp float qt_Opacity;

void main()
{
    gl_FragColor = texture2D(source, qt_TexCoord0) * qt_Opacity;
}