uniform highp mat4 MVP;

attribute highp vec3 vertexPosition_mdl;
attribute highp vec2 vertexUV;

varying highp vec2 UV;

void main() {
    gl_Position = MVP * vec4(vertexPosition_mdl, 1.0);
    UV = vertexUV;
}
