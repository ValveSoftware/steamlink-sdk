#version 150 core

uniform vec3 maincolor;
out vec4 fragColor;

void main()
{
    //output color from material
    fragColor = vec4(maincolor,1.0);
}

