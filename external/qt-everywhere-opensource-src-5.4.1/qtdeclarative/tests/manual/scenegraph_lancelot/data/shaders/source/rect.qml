import QtQuick 2.0

Item {
    width: 320
    height: 480

    Text {
        id: text
        anchors.centerIn: parent
        font.pixelSize:  80
        text: "Shaderz!"
    }

    ShaderEffectSource {
        id: source
        sourceItem: text
        sourceRect: Qt.rect(rect.x - text.x, rect.y - text.y, rect.width, rect.height)
    }

    ShaderEffect {
        anchors.fill: rect

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

    Rectangle {
        id: rect
        x: 100
        y: 100
        width: 100
        height: 100
        color: "transparent"
        border.width: 2
        border.color: "red"
        MouseArea {
            anchors.fill: parent
            drag.target: parent
            drag.axis: Drag.XAndYAxis
            drag.minimumX: 0
            drag.maximumX: parent.parent.width - width
            drag.minimumY: 0
            drag.maximumY: parent.parent.height - height
        }
    }
}
