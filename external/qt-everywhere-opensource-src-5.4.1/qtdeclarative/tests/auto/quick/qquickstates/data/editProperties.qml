import QtQuick 2.0
Rectangle {
    id: myRectangle

    property color sourceColor: "blue"
    width: 400; height: 400
    color: "red"

    Rectangle {
      id: rect2
      objectName: "rect2"
      width: parent.width + 2
      height: 200
      color: "yellow"
    }

    states: [
        State {
            name: "blue"
            PropertyChanges {
                target: rect2
                width:50
                height: 40
            }
        },
        State {
            name: "green"
            PropertyChanges {
                target: rect2
                width: myRectangle.width / 2
                height: myRectangle.width / 4
            }
        }]
}
