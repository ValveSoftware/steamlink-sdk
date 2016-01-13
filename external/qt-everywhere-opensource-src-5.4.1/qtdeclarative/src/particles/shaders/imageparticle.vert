#version 120

#if defined(DEFORM)
attribute highp vec4 vPosTex;
#else
attribute highp vec2 vPos;
#endif

attribute highp vec4 vData; // x = time, y = lifeSpan, z = size, w = endSize
attribute highp vec4 vVec;  // x,y = constant velocity,  z,w = acceleration
uniform highp float entry;

#if defined(COLOR)
attribute highp vec4 vColor;
#endif

#if defined(DEFORM)
attribute highp vec4 vDeformVec; // x,y x unit vector; z,w = y unit vector
attribute highp vec3 vRotation;  // x = radians of rotation, y = rotation velocity, z = bool autoRotate
#endif

#if defined(SPRITE)
attribute highp vec3 vAnimData; // w,h(premultiplied of anim), interpolation progress
attribute highp vec4 vAnimPos;  // x,y, x,y (two frames for interpolation)
#endif

uniform highp mat4 qt_Matrix;
uniform highp float timestamp;

#if defined(TABLE)
varying lowp vec2 tt;//y is progress if Sprite mode
uniform highp float sizetable[64];
uniform highp float opacitytable[64];
#endif

#if defined(SPRITE)
varying highp vec4 fTexS;
#elif defined(DEFORM)
varying highp vec2 fTex;
#endif

#if defined(COLOR)
varying lowp vec4 fColor;
#else
varying lowp float fFade;
#endif


void main()
{
    highp float t = (timestamp - vData.x) / vData.y;
    if (t < 0. || t > 1.) {
#if defined(DEFORM)
        gl_Position = qt_Matrix * vec4(vPosTex.x, vPosTex.y, 0., 1.);
#else
        gl_PointSize = 0.;
#endif
    } else {
#if defined(SPRITE)
        tt.y = vAnimData.z;

        // Calculate frame location in texture
        fTexS.xy = vAnimPos.xy + vPosTex.zw * vAnimData.xy;

        // Next frame is also passed, for interpolation
        fTexS.zw = vAnimPos.zw + vPosTex.zw * vAnimData.xy;

#elif defined(DEFORM)
        fTex = vPosTex.zw;
#endif
        highp float currentSize = mix(vData.z, vData.w, t * t);
#if defined (Q_OS_BLACKBERRY)
        highp float fade = 1.;
#else
        lowp float fade = 1.;
#endif
        highp float fadeIn = min(t * 10., 1.);
        highp float fadeOut = 1. - clamp((t - 0.75) * 4.,0., 1.);

#if defined(TABLE)
        currentSize = currentSize * sizetable[int(floor(t*64.))];
        fade = fade * opacitytable[int(floor(t*64.))];
#endif

        if (entry == 1.)
            fade = fade * fadeIn * fadeOut;
        else if (entry == 2.)
            currentSize = currentSize * fadeIn * fadeOut;

        if (currentSize <= 0.) {
#if defined(DEFORM)
            gl_Position = qt_Matrix * vec4(vPosTex.x, vPosTex.y, 0., 1.);
#else
            gl_PointSize = 0.;
#endif
        } else {
            if (currentSize < 3.) // Sizes too small look jittery as they move
                currentSize = 3.;

            highp vec2 pos;
#if defined(DEFORM)
            highp float rotation = vRotation.x + vRotation.y * t * vData.y;
            if (vRotation.z == 1.0){
                highp vec2 curVel = vVec.zw * t * vData.y + vVec.xy;
                if (length(curVel) > 0.)
                    rotation += atan(curVel.y, curVel.x);
            }
            highp vec2 trigCalcs = vec2(cos(rotation), sin(rotation));
            highp vec4 deform = vDeformVec * currentSize * (vPosTex.zzww - 0.5);
            highp vec4 rotatedDeform = deform.xxzz * trigCalcs.xyxy;
            rotatedDeform = rotatedDeform + (deform.yyww * trigCalcs.yxyx * vec4(-1.,1.,-1.,1.));
            /* The readable version:
            highp vec2 xDeform = vDeformVec.xy * currentSize * (vTex.x-0.5);
            highp vec2 yDeform = vDeformVec.zw * currentSize * (vTex.y-0.5);
            highp vec2 xRotatedDeform;
            xRotatedDeform.x = trigCalcs.x*xDeform.x - trigCalcs.y*xDeform.y;
            xRotatedDeform.y = trigCalcs.y*xDeform.x + trigCalcs.x*xDeform.y;
            highp vec2 yRotatedDeform;
            yRotatedDeform.x = trigCalcs.x*yDeform.x - trigCalcs.y*yDeform.y;
            yRotatedDeform.y = trigCalcs.y*yDeform.x + trigCalcs.x*yDeform.y;
            */
            pos = vPosTex.xy
                  + rotatedDeform.xy
                  + rotatedDeform.zw
                  + vVec.xy * t * vData.y         // apply velocity
                  + 0.5 * vVec.zw * pow(t * vData.y, 2.); // apply acceleration
#else
            pos = vPos
                  + vVec.xy * t * vData.y         // apply velocity vector..
                  + 0.5 * vVec.zw * pow(t * vData.y, 2.);
            gl_PointSize = currentSize;
#endif
            gl_Position = qt_Matrix * vec4(pos.x, pos.y, 0, 1);

#if defined(COLOR)
            fColor = vColor * fade;
#else
            fFade = fade;
#endif
#if defined(TABLE)
            tt.x = t;
#endif
        }
    }
}