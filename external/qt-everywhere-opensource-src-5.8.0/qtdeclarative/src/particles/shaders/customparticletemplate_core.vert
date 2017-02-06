#version 150 core

in vec2 qt_ParticlePos;
in vec2 qt_ParticleTex;
in vec4 qt_ParticleData; // x = time, y = lifeSpan, z = size, w = endSize
in vec4 qt_ParticleVec;  // x,y = constant velocity, z,w = acceleration
in float qt_ParticleR;

out vec2 qt_TexCoord0;

uniform mat4 qt_Matrix;
uniform float qt_Timestamp;

void defaultMain()
{
    qt_TexCoord0 = qt_ParticleTex;
    float size = qt_ParticleData.z;
    float endSize = qt_ParticleData.w;
    float t = (qt_Timestamp - qt_ParticleData.x) / qt_ParticleData.y;
    float currentSize = mix(size, endSize, t * t);
    if (t < 0. || t > 1.)
        currentSize = 0.;
    vec2 pos = qt_ParticlePos
             - currentSize / 2. + currentSize * qt_ParticleTex   // adjust size
             + qt_ParticleVec.xy * t * qt_ParticleData.y         // apply velocity vector..
             + 0.5 * qt_ParticleVec.zw * pow(t * qt_ParticleData.y, 2.);
    gl_Position = qt_Matrix * vec4(pos.x, pos.y, 0, 1);
}
