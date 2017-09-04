import QtQuick 2.9

Item {
    id: root

    property bool tested: false
    signal testMe()

    Connections {
        target: root
        enabled: false
        onTestMe: root.tested = true;
    }
}
