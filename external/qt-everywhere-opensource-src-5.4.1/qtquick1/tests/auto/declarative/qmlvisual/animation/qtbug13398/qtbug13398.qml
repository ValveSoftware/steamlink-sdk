import QtQuick 1.0

Item {
    width: 300
    height: 400

    Rectangle {
        id: root
        color: "darkkhaki"

        x: 50
        y: 50

        width: 200
        height: 300

        Rectangle {
            id: statusbar
            color: "chocolate"

            height: 30

            anchors.top: root.top
            anchors.left: root.left
            anchors.right: root.right
        }

        Rectangle {
            id: titlebar
            color: "crimson"

            height: 60

            anchors.top: statusbar.bottom
            anchors.left: root.left
            anchors.right: root.right
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                root.state = root.state ? "" : "fullscreen";
            }
        }

        states: [
            State {
                name: "fullscreen"
                AnchorChanges {
                    target: statusbar
                    anchors.top: undefined
                    anchors.bottom: titlebar.top
                }
                AnchorChanges {
                    target: titlebar
                    anchors.top: undefined
                    anchors.bottom: root.top
                }
            }
        ]

        transitions: [
            Transition {
                AnchorAnimation { }
            }
        ]
    }
}
