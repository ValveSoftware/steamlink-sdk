import QtQuick 2.0

Item {
    width: 320
    height: 480

    ShaderEffect {
        anchors.fill: parent;
        fragmentShader: "
        varying highp vec2 qt_TexCoord0;
        uniform lowp float qt_Opacity;
        void main() {
            gl_FragColor = vec4(qt_TexCoord0.x, qt_TexCoord0.y, 1, 1);
        }
        "
    }
}
