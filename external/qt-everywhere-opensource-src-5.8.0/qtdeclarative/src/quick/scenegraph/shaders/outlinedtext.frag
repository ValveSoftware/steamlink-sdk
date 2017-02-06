varying highp vec2 sampleCoord;
varying highp vec2 sCoordUp;
varying highp vec2 sCoordDown;
varying highp vec2 sCoordLeft;
varying highp vec2 sCoordRight;

uniform sampler2D _qt_texture;
uniform lowp vec4 color;
uniform lowp vec4 styleColor;

void main()
{
    lowp float glyph = texture2D(_qt_texture, sampleCoord).a;
    lowp float outline = clamp(clamp(texture2D(_qt_texture, sCoordUp).a +
                                     texture2D(_qt_texture, sCoordDown).a +
                                     texture2D(_qt_texture, sCoordLeft).a +
                                     texture2D(_qt_texture, sCoordRight).a,
                                     0.0, 1.0) - glyph,
                               0.0, 1.0);
    gl_FragColor = outline * styleColor + glyph * color;
}