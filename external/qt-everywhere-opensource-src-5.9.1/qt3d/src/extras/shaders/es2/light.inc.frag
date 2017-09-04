const int MAX_LIGHTS = 8;
const int TYPE_POINT = 0;
const int TYPE_DIRECTIONAL = 1;
const int TYPE_SPOT = 2;
struct Light {
    int type;
    FP vec3 position;
    FP vec3 color;
    FP float intensity;
    FP vec3 direction;
    FP vec3 attenuation;
    FP float cutOffAngle;
};
uniform Light lights[MAX_LIGHTS];
uniform int lightCount;

void adsModelNormalMapped(const in FP vec3 vpos, const in FP vec3 vnormal, const in FP vec3 eye, const in FP float shininess,
                          const in FP mat3 tangentMatrix,
                          out FP vec3 diffuseColor, out FP vec3 specularColor)
{
    diffuseColor = vec3(0.0);
    specularColor = vec3(0.0);

    FP vec3 snormal = normalize( vec3( tangentMatrix[0][2], tangentMatrix[1][2], tangentMatrix[2][2] ) );

    FP vec3 n = normalize( vnormal );

    FP vec3 s, ts;
    Light light;
    for (int i = 0; i < lightCount; ++i) {
        if (i == 0)
            light = lights[0];
        else if (i == 1)
            light = lights[1];
        else if (i == 2)
            light = lights[2];
        else if (i == 3)
            light = lights[3];
        else if (i == 4)
            light = lights[4];
        else if (i == 5)
            light = lights[5];
        else if (i == 6)
            light = lights[6];
        else if (i == 7)
            light = lights[7];

        FP float att = 1.0;
        if ( light.type != TYPE_DIRECTIONAL ) {
            s = light.position - vpos;
            if ( dot(snormal, s) < 0.0 )
                att = 0.0;
            else {
                ts = normalize( tangentMatrix * s );
                if (length( light.attenuation ) != 0.0) {
                    FP float dist = length(s);
                    att = 1.0 / (light.attenuation.x + light.attenuation.y * dist + light.attenuation.z * dist * dist);
                }
                s = normalize( s );
                if ( light.type == TYPE_SPOT ) {
                    if ( degrees(acos(dot(-s, normalize(light.direction))) ) > light.cutOffAngle)
                        att = 0.0;
                }
            }
        } else {
            if ( dot(snormal, -light.direction) > 0.0 )
                s = normalize( tangentMatrix * -light.direction );
            else
                att = 0.0;
        }

        FP float diffuse = max( dot( ts, n ), 0.0 );

        FP float specular = 0.0;
        if (diffuse > 0.0 && shininess > 0.0 && att > 0.0) {
            FP vec3 r = reflect( -ts, n );
            FP vec3 v = normalize( tangentMatrix * ( eye - vpos ) );
            FP float normFactor = ( shininess + 2.0 ) / 2.0;
            specular = normFactor * pow( max( dot( r, v ), 0.0 ), shininess );
        }

        diffuseColor += att * light.intensity * diffuse * light.color;
        specularColor += att * light.intensity * specular * light.color;
    }
}

void adsModel(const in FP vec3 vpos, const in FP vec3 vnormal, const in FP vec3 eye, const in FP float shininess,
              out FP vec3 diffuseColor, out FP vec3 specularColor)
{
    diffuseColor = vec3(0.0);
    specularColor = vec3(0.0);

    FP vec3 n = normalize( vnormal );

    FP vec3 s;
    Light light;
    for (int i = 0; i < lightCount; ++i) {
        if (i == 0)
            light = lights[0];
        else if (i == 1)
            light = lights[1];
        else if (i == 2)
            light = lights[2];
        else if (i == 3)
            light = lights[3];
        else if (i == 4)
            light = lights[4];
        else if (i == 5)
            light = lights[5];
        else if (i == 6)
            light = lights[6];
        else if (i == 7)
            light = lights[7];

        FP float att = 1.0;
        if ( light.type != TYPE_DIRECTIONAL ) {
            s = light.position - vpos;
            if (length( light.attenuation ) != 0.0) {
                FP float dist = length(s);
                att = 1.0 / (light.attenuation.x + light.attenuation.y * dist + light.attenuation.z * dist * dist);
            }
            s = normalize( s );
            if ( light.type == TYPE_SPOT ) {
                if ( degrees(acos(dot(-s, normalize(light.direction))) ) > light.cutOffAngle)
                    att = 0.0;
            }
        } else {
            s = normalize( -light.direction );
        }

        FP float diffuse = max( dot( s, n ), 0.0 );

        FP float specular = 0.0;
        if (diffuse > 0.0 && shininess > 0.0 && att > 0.0) {
            FP vec3 r = reflect( -s, n );
            FP vec3 v = normalize( eye - vpos );
            FP float normFactor = ( shininess + 2.0 ) / 2.0;
            specular = normFactor * pow( max( dot( r, v ), 0.0 ), shininess );
        }

        diffuseColor += att * light.intensity * diffuse * light.color;
        specularColor += att * light.intensity * specular * light.color;
    }
}

void adModel(const in FP vec3 vpos, const in FP vec3 vnormal, out FP vec3 diffuseColor)
{
    diffuseColor = vec3(0.0);

    FP vec3 n = normalize( vnormal );

    FP vec3 s;
    Light light;
    for (int i = 0; i < lightCount; ++i) {
        if (i == 0)
            light = lights[0];
        else if (i == 1)
            light = lights[1];
        else if (i == 2)
            light = lights[2];
        else if (i == 3)
            light = lights[3];
        else if (i == 4)
            light = lights[4];
        else if (i == 5)
            light = lights[5];
        else if (i == 6)
            light = lights[6];
        else if (i == 7)
            light = lights[7];

        FP float att = 1.0;
        if ( light.type != TYPE_DIRECTIONAL ) {
            s = light.position - vpos;
            if (length( light.attenuation ) != 0.0) {
                FP float dist = length(s);
                att = 1.0 / (light.attenuation.x + light.attenuation.y * dist + light.attenuation.z * dist * dist);
            }
            s = normalize( s );
            if ( light.type == TYPE_SPOT ) {
                if ( degrees(acos(dot(-s, normalize(light.direction))) ) > light.cutOffAngle)
                    att = 0.0;
            }
        } else {
            s = normalize( -light.direction );
        }

        FP float diffuse = max( dot( s, n ), 0.0 );

        diffuseColor += att * light.intensity * diffuse * light.color;
    }
}
