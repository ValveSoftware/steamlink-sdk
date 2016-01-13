uniform sampler2D _qt_texture;
uniform lowp float qt_Opacity;

varying highp vec4 fTexS;
varying lowp float progress;

void main()
{
    gl_FragColor = mix(texture2D(_qt_texture, fTexS.xy),
                       texture2D(_qt_texture, fTexS.zw),
                       progress) * qt_Opacity;
}