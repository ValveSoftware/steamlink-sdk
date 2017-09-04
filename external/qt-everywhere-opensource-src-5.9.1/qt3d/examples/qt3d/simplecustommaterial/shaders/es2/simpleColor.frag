#define FP highp

uniform FP vec3 maincolor;

void main()
{
    //output color from material
    gl_FragColor = vec4(maincolor,1.0);
}

