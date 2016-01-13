import QtQuick 1.0

Rectangle {
    width: 400; height: 400

    Double { id: dub; on: parent.width < 800 }
    Component.onCompleted: dub.on = false
}
