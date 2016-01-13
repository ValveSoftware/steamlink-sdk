import QtQuick 1.0

QtObject {
    id: root

    property QtObject nullObject

    property bool test1: Qt.isQtObject(root)
    property bool test2: Qt.isQtObject(nullObject)
    property bool test3: Qt.isQtObject(10)
    property bool test4: Qt.isQtObject(null)
    property bool test5: Qt.isQtObject({ a: 10, b: 11 })
}

