import QtQuick 2.0

Item {
  width: 320
  height: 400

  Flickable {
    objectName: "flickable"
    width: 100
    height: 400

    flickableDirection: Flickable.VerticalFlick
    contentHeight: 800

    Rectangle {
        property bool highlight: false
        width: 300
        height: 350
        color: "green"

        MultiPointTouchArea {
            anchors.fill: parent
            minimumTouchPoints: 1
            maximumTouchPoints: 1
            touchPoints: [ TouchPoint { id: point1; objectName: "point1" } ]
        }
    }

  }
}
