import QtQuick 2.0
Item {
    anchors {
        margins: 50
        Behavior on margins {
            NumberAnimation { duration: 10 }
        }
    }
}
