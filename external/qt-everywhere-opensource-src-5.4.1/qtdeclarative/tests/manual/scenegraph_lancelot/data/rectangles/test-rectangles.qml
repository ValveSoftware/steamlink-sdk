import QtQuick 2.0

Rectangle {
    width: 320
    height: 480
    Column {
        id: foo
        x: -width * (scale - 1) * (10/9) * (mouseArea.mouseX / width - 0.5)
        y: -height * (scale - 1) * (10/9) * (mouseArea.mouseY / height - 0.5)
        states: [
            State {
                name: ""
                PropertyChanges {
                    target: foo
                    scale: 1
                }
            },
            State {
                name: "zoomed"
                when: mouseArea.pressed
                PropertyChanges {
                    target: foo
                    scale: 10
                }
            }
        ]
        Behavior on scale {
            NumberAnimation { duration: 300; easing.type: Easing.InOutSine }
        }

        Repeater {
            model: 3
            Row {
                id: local
                property int _index: index
                Repeater {
                    model: 2
                    Item {
                        width: 80
                        height: 160
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: 10
                            radius: index * 20
                            border.pixelAligned: local._index == 1
                            border.width: local._index == 0 ? 0 : 0.5
                            opacity: 0.5
                            color: "steelBlue"
                        }
                    }
                }
                Repeater {
                    model: 2
                    Item {
                        width: 80
                        height: 160
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: 10
                            radius: index * 20
                            border.pixelAligned: local._index == 1
                            border.width: local._index == 0 ? 0 : 0.5
                            opacity: 0.5
                            gradient: Gradient {
                                GradientStop { position: 0.05; color: "lightsteelblue" }
                                GradientStop { position: 0.1; color: "lightskyblue" }
                                GradientStop { position: 0.5; color: "skyblue" }
                                GradientStop { position: 0.9; color: "deepskyblue" }
                                GradientStop { position: 0.95; color: "dodgerblue" }
                            }
                        }
                    }
                }
            }
        }
    }
    MouseArea {
        id: mouseArea
        anchors.fill: parent
    }
}
