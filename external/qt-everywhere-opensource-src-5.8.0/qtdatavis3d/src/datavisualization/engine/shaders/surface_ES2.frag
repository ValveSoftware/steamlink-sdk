varying highp vec2 UV;
varying highp vec2 coords_mdl;
varying highp vec3 position_wrld;
varying highp vec3 normal_cmr;
varying highp vec3 eyeDirection_cmr;
varying highp vec3 lightDirection_cmr;
varying highp vec3 lightPosition_wrld_frag;

uniform sampler2D textureSampler;
uniform highp float lightStrength;
uniform highp float ambientStrength;
uniform highp vec4 lightColor;
uniform highp float gradMin;
uniform highp float gradHeight;

void main() {
    highp vec2 gradientUV = vec2(0.0, gradMin + coords_mdl.y * gradHeight);
    highp vec3 materialDiffuseColor = texture2D(textureSampler, gradientUV).xyz;
    highp vec3 materialAmbientColor = lightColor.rgb * ambientStrength * materialDiffuseColor;
    highp vec3 materialSpecularColor = lightColor.rgb;

    highp float distance = length(lightPosition_wrld_frag - position_wrld);

    highp vec3 n = normalize(normal_cmr);
    highp vec3 l = normalize(lightDirection_cmr);
    highp float cosTheta = dot(n, l);
    if (cosTheta < 0.0) cosTheta = 0.0;
    else if (cosTheta > 1.0) cosTheta = 1.0;

    highp vec3 E = normalize(eyeDirection_cmr);
    highp vec3 R = reflect(-l, n);
    highp float cosAlpha = dot(E, R);
    if (cosAlpha < 0.0) cosAlpha = 0.0;
    else if (cosAlpha > 1.0) cosAlpha = 1.0;

    gl_FragColor.rgb =
        materialAmbientColor +
        materialDiffuseColor * lightStrength * cosTheta * cosTheta / distance +
        materialSpecularColor * lightStrength * cosAlpha * cosAlpha * cosAlpha * cosAlpha * cosAlpha * cosAlpha * cosAlpha * cosAlpha * cosAlpha * cosAlpha / distance;
    gl_FragColor.a = 1.0;
}

