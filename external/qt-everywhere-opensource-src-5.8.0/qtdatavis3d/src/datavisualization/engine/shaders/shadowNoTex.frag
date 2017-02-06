#version 120

uniform highp float lightStrength;
uniform highp float ambientStrength;
uniform highp float shadowQuality;
uniform highp vec4 color_mdl;
uniform highp sampler2DShadow shadowMap;
uniform highp vec4 lightColor;

varying highp vec4 shadowCoord;
varying highp vec3 position_wrld;
varying highp vec3 normal_cmr;
varying highp vec3 eyeDirection_cmr;
varying highp vec3 lightDirection_cmr;

highp vec2 poissonDisk[16] = vec2[16](vec2(-0.94201624, -0.39906216),
                                      vec2(0.94558609, -0.76890725),
                                      vec2(-0.094184101, -0.92938870),
                                      vec2(0.34495938, 0.29387760),
                                      vec2(-0.91588581, 0.45771432),
                                      vec2(-0.81544232, -0.87912464),
                                      vec2(-0.38277543, 0.27676845),
                                      vec2(0.97484398, 0.75648379),
                                      vec2(0.44323325, -0.97511554),
                                      vec2(0.53742981, -0.47373420),
                                      vec2(-0.26496911, -0.41893023),
                                      vec2(0.79197514, 0.19090188),
                                      vec2(-0.24188840, 0.99706507),
                                      vec2(-0.81409955, 0.91437590),
                                      vec2(0.19984126, 0.78641367),
                                      vec2(0.14383161, -0.14100790));

void main() {
    highp vec3 materialDiffuseColor = color_mdl.rgb;
    highp vec3 materialAmbientColor = lightColor.rgb * ambientStrength * materialDiffuseColor;
    highp vec3 materialSpecularColor = lightColor.rgb;

    highp vec3 n = normalize(normal_cmr);
    highp vec3 l = normalize(lightDirection_cmr);
    highp float cosTheta = clamp(dot(n, l), 0.0, 1.0);

    highp vec3 E = normalize(eyeDirection_cmr);
    highp vec3 R = reflect(-l, n);
    highp float cosAlpha = clamp(dot(E, R), 0.0, 1.0);

    highp float bias = 0.005 * tan(acos(cosTheta));
    bias = clamp(bias, 0.0, 0.01);

    vec4 shadCoords = shadowCoord;
    shadCoords.z -= bias;

    highp float visibility = 0.6;
    for (int i = 0; i < 15; i++) {
        vec4 shadCoordsPD = shadCoords;
        shadCoordsPD.x += cos(poissonDisk[i].x) / shadowQuality;
        shadCoordsPD.y += sin(poissonDisk[i].y) / shadowQuality;
        visibility += 0.025 * shadow2DProj(shadowMap, shadCoordsPD).r;
    }

    gl_FragColor.rgb =
        (materialAmbientColor +
        materialDiffuseColor * lightStrength * cosTheta +
        materialSpecularColor * lightStrength * pow(cosAlpha, 10));
    gl_FragColor.a = color_mdl.a;
    gl_FragColor.rgb = visibility * clamp(gl_FragColor.rgb, 0.0, 1.0);
}
