import QtQuick 2.0

Rectangle {
    width: 200
    height: 200
    color: "blue"
    layer.enabled: true
    layer.effect: ShaderEffect {
        fragmentShader: "
            uniform sampler2D foo;
            uniform lowp float qt_Opacity;
            varying highp vec2 qt_TexCoord0;
            void main() {
                gl_FragColor = texture2D(foo, qt_TexCoord0) * qt_Opacity;
            }"
    }
    Component.onCompleted: layer.samplerName = "foo"
}
