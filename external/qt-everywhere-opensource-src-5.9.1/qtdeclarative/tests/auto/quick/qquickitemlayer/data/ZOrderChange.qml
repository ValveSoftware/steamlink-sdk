import QtQuick 2.0

Item {
    width: 200
    height: 200
    property bool layerEffect: false
    property bool layerEnabled: false
    property real layerZ: 0
    Rectangle {
        anchors.fill: parent
        color: "#00ffff"
    }
    Rectangle {
        id: foo
        anchors.fill: parent
        color: "#ffff00"
        Rectangle {
            width: 100
            height: 100
            color: "#00ffff"
        }
        layer.enabled: parent.layerEnabled
        layer.effect: parent.layerEffect ? effectComponent : null
        opacity: 0.5
        z: layerZ
    }
    Rectangle {
        width: 100
        height: 100
        x: 100
        color: "#ff0000"
    }
    Rectangle {
        width: 100
        height: 100
        y: 100
        color: "#0000ff"
        z: 1
    }
    Component {
        id: effectComponent
        ShaderEffect {
            fragmentShader: "
                uniform sampler2D source;
                uniform lowp float qt_Opacity;
                varying highp vec2 qt_TexCoord0;
                void main() { gl_FragColor = texture2D(source, qt_TexCoord0).xzyw * qt_Opacity; }"
        }
    }
}
