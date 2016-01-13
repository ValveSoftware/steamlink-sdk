#version 120

uniform sampler2D _qt_texture;
uniform lowp float qt_Opacity;

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

#if defined(TABLE)
varying lowp vec2 tt;
uniform sampler2D colortable;
#endif

void main()
{
#if defined(SPRITE)
    gl_FragColor = mix(texture2D(_qt_texture, fTexS.xy), texture2D(_qt_texture, fTexS.zw), tt.y)
            * fColor
            * texture2D(colortable, tt)
            * qt_Opacity;
#elif defined(TABLE)
    gl_FragColor = texture2D(_qt_texture, fTex)
            * fColor
            * texture2D(colortable, tt)
            * qt_Opacity;
#elif defined(DEFORM)
    gl_FragColor = (texture2D(_qt_texture, fTex)) * fColor * qt_Opacity;
#elif defined(COLOR)
    gl_FragColor = (texture2D(_qt_texture, gl_PointCoord)) * fColor * qt_Opacity;
#else
    gl_FragColor = texture2D(_qt_texture, gl_PointCoord) * (fFade * qt_Opacity);
#endif
}