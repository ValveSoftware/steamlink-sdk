import QtQuick 1.0
import "../shared" 1.0

Rectangle {
    resources: [
        Component { id: cursorA
            Item { id: cPage;
                Behavior on x { NumberAnimation { } }
                Behavior on y { NumberAnimation { } }
                Behavior on height { NumberAnimation { duration: 200 } }
                Rectangle { id: cRectangle; color: "black"; y: 1; width: 1; height: parent.height-2;
                    Rectangle { id:top; color: "black"; width: 3; height: 1; x: -1; y:0}
                    Rectangle { id:bottom; color: "black"; width: 3; height: 1; x: -1; anchors.bottom: parent.bottom;}
                }
                SequentialAnimation on opacity { running: true; loops: Animation.Infinite;
                    NumberAnimation { to: 0; duration: 500; easing.type: "OutQuad"}
                    NumberAnimation { to: 1; duration: 500; easing.type: "InQuad"}
                }
                width: 1;
            }
        }
    ]
    width: 400
    height: 200
    color: "white"
    TestTextEdit { id: mainText
        text: "Hello World"
        cursorDelegate: cursorA
        focus: true
        font.pixelSize: 28
        selectionColor: "lightsteelblue"
        selectedTextColor: "deeppink"
        color: "forestgreen"
        anchors.centerIn: parent
    }
}
