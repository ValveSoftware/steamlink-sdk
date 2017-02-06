attribute highp vec4 qt_Vertex;
attribute highp vec2 qt_MultiTexCoord0;

uniform highp mat4 qt_Matrix;
uniform highp float bend;
uniform highp float minimize;
uniform highp float side;
uniform highp float width;
uniform highp float height;

varying highp vec2 qt_TexCoord0;

void main() {
    qt_TexCoord0 = qt_MultiTexCoord0;
    highp vec4 pos = qt_Vertex;
    pos.y = mix(qt_Vertex.y, height, minimize);
    highp float t = pos.y / height;
    t = (3. - 2. * t) * t * t;
    pos.x = mix(qt_Vertex.x, side * width, t * bend);
    gl_Position = qt_Matrix * pos;
}
