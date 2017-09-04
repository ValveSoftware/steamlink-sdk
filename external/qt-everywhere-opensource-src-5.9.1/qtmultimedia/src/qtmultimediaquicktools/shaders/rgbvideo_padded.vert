uniform highp mat4 qt_Matrix;
uniform highp float width;
attribute highp vec4 qt_VertexPosition;
attribute highp vec2 qt_VertexTexCoord;
varying highp vec2 qt_TexCoord;

void main() {
    qt_TexCoord = qt_VertexTexCoord * vec2(width, 1);
    gl_Position = qt_Matrix * qt_VertexPosition;
}
