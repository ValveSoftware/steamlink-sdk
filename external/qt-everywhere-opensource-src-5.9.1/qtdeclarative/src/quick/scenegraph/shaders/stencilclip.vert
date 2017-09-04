attribute highp vec4 vCoord;

uniform highp mat4 matrix;

void main()
{
    gl_Position = matrix * vCoord;
}