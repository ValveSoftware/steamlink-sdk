uniform highp mat4 qt_Matrix;
uniform highp float plane1Width;
uniform highp float plane2Width;
uniform highp float plane3Width;
attribute highp vec4 qt_VertexPosition;
attribute highp vec2 qt_VertexTexCoord;
varying highp vec2 plane1TexCoord;
varying highp vec2 plane2TexCoord;
varying highp vec2 plane3TexCoord;

void main() {
    plane1TexCoord = qt_VertexTexCoord * vec2(plane1Width, 1);
    plane2TexCoord = qt_VertexTexCoord * vec2(plane2Width, 1);
    plane3TexCoord = qt_VertexTexCoord * vec2(plane3Width, 1);
    gl_Position = qt_Matrix * qt_VertexPosition;
}
