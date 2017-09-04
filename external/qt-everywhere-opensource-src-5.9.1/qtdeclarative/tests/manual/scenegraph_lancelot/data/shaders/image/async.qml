import QtQuick 2.0

Item {
    width: 320
    height: 480

    Image {
        id: image;
        source: "./face-smile.png"
        visible: false
        asynchronous: true
    }

    ShaderEffect {
        anchors.fill: image
        property variant source: image

        fragmentShader: "
        uniform lowp sampler2D source;
        varying highp vec2 qt_TexCoord0;
        uniform lowp float qt_Opacity;
        void main() {
            gl_FragColor = vec4(qt_TexCoord0.x, qt_TexCoord0.y, 0, 1) + texture2D(source, qt_TexCoord0);
        }
        "
        visible: image.status == Image.Ready
    }
}
