import QtQuick 1.0

Item {
    id: screen; width: 50

    property bool tested: false
    signal testMe

    Connections { target: screen; onWidthChanged: screen.tested = true }
}
