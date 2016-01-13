import QtQuick 2.0

Rectangle {
    id: topLevel
    width: 320
    height: 480
    ShaderEffectSource {
        id: front
        visible: false
        smooth: true
        sourceItem: Rectangle {
            width: 256
            height: 64
            color: "cornflowerblue"
            radius: 8
            Text {
                anchors.centerIn: parent
                text: "Front"
                font.pixelSize: 48
                color: "white"
            }
        }
    }
    ShaderEffectSource {
        id: back
        visible: false
        smooth: true
        sourceItem: Rectangle {
            width: 256
            height: 64
            color: "firebrick"
            radius: 8
            Text {
                anchors.centerIn: parent
                text: "Back"
                font.pixelSize: 48
                color: "white"
            }
        }
    }
    Column {
        anchors.fill: parent
        Repeater {
            model: ListModel {
                ListElement {
                    foo: "No culling"
                    bar: ShaderEffect.NoCulling
                    turned: false
                }
                ListElement {
                    foo: "Back-face culling"
                    bar: ShaderEffect.BackFaceCulling
                    turned: false
                }
                ListElement {
                    foo: "Front-face culling"
                    bar: ShaderEffect.FrontFaceCulling
                    turned: false
                }
            }

            Item{
                id: item_0000
                width: 320
                height: 120
                ShaderEffect {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: 10
                    width: 200
                    height: 100
                    cullMode: model.bar
                    property variant frontSource: front
                    property variant backSource: back
                    fragmentShader: "
                            varying highp vec2 qt_TexCoord0;
                            uniform sampler2D frontSource;
                            uniform sampler2D backSource;
                            uniform lowp float qt_Opacity;
                            void main() {
                                gl_FragColor = gl_FrontFacing
                                             ? texture2D(frontSource, qt_TexCoord0)
                                             : texture2D(backSource, qt_TexCoord0);
                            }"
                    transform: Rotation {
                        origin.x: 200
                        origin.y: 180 - 120 * index
                        axis { x: 0; y: 1; z: 0 }
                        angle: (turned == true) ? 180 : 0

                    }
                }
                Text {
                    font.pointSize: 10
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.margins: 5
                    text: foo
                }
            }
        }
    }
}

