import QtQuick 2.0

import "importScriptsWithoutQmlMode.js" as Export

Rectangle {
    id: root
    property bool success: false

    Component.onCompleted: success = Export.isLegal()
}
