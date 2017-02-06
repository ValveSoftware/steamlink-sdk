attribute highp vec4 vertexCoord;
attribute highp vec4 vertexColor;

uniform highp mat4 matrix;
uniform highp float opacity;

varying lowp vec4 color;

void main()
{
    gl_Position = matrix * vertexCoord;
    color = vertexColor * opacity;
}