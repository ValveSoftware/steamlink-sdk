import QtQuick 2.0

Item {
    width: 320
    height: 480

    Rectangle {
        id: rect1
        x: 10
        y: 10
        width: 80
        height: 80
        radius: 20
        color: "black"
    }

    Rectangle {
        id: rect2
        x: 100
        y: 10
        width: 80
        height: 80
        radius: 20
        color: "black"
    }

    Rectangle {
        id: rect3
        x: 190
        y: 10
        width: 80
        height: 80
        radius: 20
        color: "black"
    }

    ShaderEffectSource {
        id: source
        property int counter
        sourceItem: rect2
        hideSource: true
    }

    ShaderEffect {
        id: effect
        anchors.fill: source.sourceItem

        property variant source: source

        fragmentShader: "
        uniform lowp sampler2D source;
        varying highp vec2 qt_TexCoord0;
        uniform lowp float qt_Opacity;
        void main() {
            gl_FragColor = vec4(qt_TexCoord0.x, qt_TexCoord0.y, 1, 1) * texture2D(source, qt_TexCoord0).a;
        }
        "
    }


}
