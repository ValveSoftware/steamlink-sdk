#version 120

uniform highp mat4 MVP;
uniform highp mat4 V;
uniform highp mat4 M;
uniform highp mat4 itM;
uniform highp mat4 depthMVP;
uniform highp vec3 lightPosition_wrld;

attribute highp vec3 vertexPosition_mdl;
attribute highp vec3 vertexNormal_mdl;
attribute highp vec2 vertexUV;

varying highp vec2 UV;
varying highp vec3 position_wrld;
varying highp vec3 normal_cmr;
varying highp vec3 eyeDirection_cmr;
varying highp vec3 lightDirection_cmr;
varying highp vec4 shadowCoord;
varying highp vec2 coords_mdl;

const highp mat4 bias = mat4(0.5, 0.0, 0.0, 0.0,
                             0.0, 0.5, 0.0, 0.0,
                             0.0, 0.0, 0.5, 0.0,
                             0.5, 0.5, 0.5, 1.0);

void main() {
    gl_Position = MVP * vec4(vertexPosition_mdl, 1.0);
    coords_mdl = vertexPosition_mdl.xy;
    shadowCoord = bias * depthMVP * vec4(vertexPosition_mdl, 1.0);
    position_wrld = vec4(M * vec4(vertexPosition_mdl, 1.0)).xyz;
    vec3 vertexPosition_cmr = vec4(V * M * vec4(vertexPosition_mdl, 1.0)).xyz;
    eyeDirection_cmr = vec3(0.0, 0.0, 0.0) - vertexPosition_cmr;
    lightDirection_cmr = vec4(V * vec4(lightPosition_wrld, 0.0)).xyz;
    normal_cmr = vec4(V * itM * vec4(vertexNormal_mdl, 0.0)).xyz;
    UV = vertexUV;
}
