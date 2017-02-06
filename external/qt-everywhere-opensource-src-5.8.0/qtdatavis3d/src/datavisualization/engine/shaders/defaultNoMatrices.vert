attribute highp vec3 vertexPosition_mdl;
attribute highp vec2 vertexUV;
attribute highp vec3 vertexNormal_mdl;

uniform highp mat4 MVP;
uniform highp mat4 V;
uniform highp vec3 lightPosition_wrld;

varying highp vec3 lightPosition_wrld_frag;
varying highp vec3 position_wrld;
varying highp vec3 normal_cmr;
varying highp vec3 eyeDirection_cmr;
varying highp vec3 lightDirection_cmr;
varying highp vec2 coords_mdl;

void main() {
    gl_Position = MVP * vec4(vertexPosition_mdl, 1.0);
    coords_mdl = vertexPosition_mdl.xy;
    position_wrld = vertexPosition_mdl;
    vec3 vertexPosition_cmr = vec4(V * vec4(vertexPosition_mdl, 1.0)).xyz;
    eyeDirection_cmr = vec3(0.0, 0.0, 0.0) - vertexPosition_cmr;
    vec3 lightPosition_cmr = vec4(V * vec4(lightPosition_wrld, 1.0)).xyz;
    lightDirection_cmr = lightPosition_cmr + eyeDirection_cmr;
    normal_cmr = vec4(V * vec4(vertexNormal_mdl, 0.0)).xyz;
    lightPosition_wrld_frag = lightPosition_wrld;
}
