uniform lowp sampler2D source;
uniform lowp float qt_Opacity;

varying highp vec2 qt_TexCoord0;
varying highp vec2 qt_TexCoord1;
varying highp vec2 qt_TexCoord2;
varying highp vec2 qt_TexCoord3;

void main() {
    highp vec4 sourceColor = (texture2D(source, qt_TexCoord0) +
    texture2D(source, qt_TexCoord1) +
    texture2D(source, qt_TexCoord2) +
    texture2D(source, qt_TexCoord3)) * 0.25;
    gl_FragColor = sourceColor * qt_Opacity;
}
