import QtQuick 2.0

QtObject {
    property int returnCode: -1
    Component.onCompleted: Qt.exit(returnCode)
}

