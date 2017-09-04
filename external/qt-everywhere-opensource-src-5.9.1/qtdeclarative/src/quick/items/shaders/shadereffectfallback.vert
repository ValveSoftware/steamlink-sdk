uniform highp mat4 qt_Matrix;

attribute highp vec4 v;

void main()
{
    gl_Position = qt_Matrix * v;
}