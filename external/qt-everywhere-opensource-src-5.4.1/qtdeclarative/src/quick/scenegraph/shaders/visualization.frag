uniform lowp vec4 color;
uniform mediump vec4 tweak; // x,y -> width, height; z -> intensity of ;

varying mediump vec2 pos;

void main(void)
{
    lowp vec4 c = color;
    c.xyz += pow(max(sin(pos.x + pos.y), 0.0), 2.0) * tweak.z * 0.25;
    gl_FragColor = c;
}
