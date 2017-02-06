uniform lowp vec4 color;
uniform lowp float pattern;

varying mediump vec2 pos;

void main(void)
{
    lowp vec4 c = color;
    c.xyz += pow(max(sin(pos.x + pos.y), 0.0), 2.0) * pattern * 0.25;
    gl_FragColor = c;
}
