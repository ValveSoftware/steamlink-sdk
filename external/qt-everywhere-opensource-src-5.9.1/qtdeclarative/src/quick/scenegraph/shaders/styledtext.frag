varying highp vec2 sampleCoord;
varying highp vec2 shiftedSampleCoord;

uniform sampler2D _qt_texture;
uniform lowp vec4 color;
uniform lowp vec4 styleColor;

void main()
{
    lowp float glyph = texture2D(_qt_texture, sampleCoord).a;
    lowp float style = clamp(texture2D(_qt_texture, shiftedSampleCoord).a - glyph,
                             0.0, 1.0);
    gl_FragColor = style * styleColor + glyph * color;
}