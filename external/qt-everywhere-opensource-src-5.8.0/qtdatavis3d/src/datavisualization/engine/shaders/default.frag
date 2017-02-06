#version 120

varying highp vec2 UV;
varying highp vec2 coords_mdl;
varying highp vec3 position_wrld;
varying highp vec3 normal_cmr;
varying highp vec3 eyeDirection_cmr;
varying highp vec3 lightDirection_cmr;

uniform highp vec3 lightPosition_wrld;
uniform highp vec4 color_mdl;
uniform highp float lightStrength;
uniform highp float ambientStrength;
uniform highp vec4 lightColor;

void main() {
    highp vec3 materialDiffuseColor = color_mdl.rgb;
    highp vec3 materialAmbientColor = lightColor.rgb * ambientStrength * materialDiffuseColor;
    highp vec3 materialSpecularColor = lightColor.rgb;

    highp float distance = length(lightPosition_wrld - position_wrld);

    highp vec3 n = normalize(normal_cmr);
    highp vec3 l = normalize(lightDirection_cmr);
    highp float cosTheta = clamp(dot(n, l), 0.0, 1.0);

    highp vec3 E = normalize(eyeDirection_cmr);
    highp vec3 R = reflect(-l, n);
    highp float cosAlpha = clamp(dot(E, R), 0.0, 1.0);

    gl_FragColor.rgb =
        materialAmbientColor +
        materialDiffuseColor * lightStrength * pow(cosTheta, 2) / distance +
        materialSpecularColor * lightStrength * pow(cosAlpha, 5) / distance;
    gl_FragColor.a = color_mdl.a;
    gl_FragColor = clamp(gl_FragColor, 0.0, 1.0);
}

