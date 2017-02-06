attribute highp vec4 av;
attribute highp vec2 at;

varying highp vec2 t;

void main()
{
    gl_Position = av;
    t = at;
}