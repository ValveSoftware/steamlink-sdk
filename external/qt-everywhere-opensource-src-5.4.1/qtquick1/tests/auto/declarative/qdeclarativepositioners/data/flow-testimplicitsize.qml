import QtQuick 1.1

Rectangle {
    width: 300; height: 200;

    property int flowLayout: 1

    Flow {
        objectName: "flow"
        layoutDirection: (flowLayout == 2) ? Qt.RightToLeft : Qt.LeftToRight
        flow: (flowLayout == 1) ? Flow.TopToBottom : Flow.LeftToRight;

        spacing: 20
        anchors.horizontalCenter: parent.horizontalCenter
        Rectangle { color: "red"; width: 100; height: 50 }
        Rectangle { color: "blue"; width: 100; height: 50 }
    }
}

