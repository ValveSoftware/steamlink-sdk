varying highp vec2 v_texcoord;
uniform highp sampler2D tex0;
uniform lowp float qt_Opacity;

void main() {
   gl_FragColor.rgb = qt_Opacity * texture2D(tex0, v_texcoord).rgb;
   gl_FragColor.a = qt_Opacity;
}
