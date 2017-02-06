varying highp vec2 sampleCoord;

uniform lowp sampler2D _qt_texture;
uniform lowp vec4 color;

void main()
{
    gl_FragColor = color * texture2D(_qt_texture, sampleCoord).a;
}