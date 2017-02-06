import QtQuick 2.0

Rectangle {
    id: root

    Component.onCompleted: {
        var o = Qt.createQmlObject("import QtQuick 2.0; \
        \
        Item { \
            property bool b: true; \
        }", root, "Instance")
    }
}
