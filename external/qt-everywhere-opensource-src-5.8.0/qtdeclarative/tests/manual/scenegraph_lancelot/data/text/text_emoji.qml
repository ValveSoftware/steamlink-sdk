import QtQuick 2.0

Item {
    width: 320
    height: 480

    Component {
        id: component
        Column {
            property variant listModel: model
            Repeater {
                model: [Text.NativeRendering, Text.QtRendering]
                Rectangle {
                    width: text.implicitWidth
                    height: text.implicitHeight
                    color: listModel.backGroundColor ? listModel.backGroundColor : "white"

                    Text {
                        id: text
                        font.pixelSize: 32
                        renderType: modelData
                        text: "e😃m😇o😍j😜i😸!"

                        color: listModel.color ? listModel.color : "black"
                        opacity: listModel.opacity ? listModel.opacity : 1.0
                    }
                }
            }
        }
    }

    Column {
        anchors.centerIn: parent
        Repeater {
            model: ListModel {
                ListElement { color: "black" }
                ListElement { color: "blue" }
                ListElement { color: "#990000ff" }
                ListElement { opacity: 0.5 }
                ListElement { backGroundColor: "green" }
            }
            delegate: component
        }
    }
}
