uniform highp mat4 qt_Matrix;
attribute highp vec2 qt_VertexPosition;
attribute highp vec2 qt_VertexTexCoord;
varying highp vec2 v_texcoord;

void main() {
   gl_Position = qt_Matrix * vec4(qt_VertexPosition, 0.0, 1.0);
   v_texcoord = qt_VertexTexCoord;
}
