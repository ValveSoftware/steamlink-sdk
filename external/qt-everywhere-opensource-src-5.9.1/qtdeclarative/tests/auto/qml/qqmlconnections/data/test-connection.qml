import QtQuick 2.0

Item {
    id: screen; width: 50

    property bool tested: false
    signal testMe

    Connections { objectName: "connections"; target: screen; onWidthChanged: screen.tested = true }
}
