uniform lowp sampler2D tex;

varying highp vec2 t;

void main()
{
    gl_FragColor = texture2D(tex, t);
}