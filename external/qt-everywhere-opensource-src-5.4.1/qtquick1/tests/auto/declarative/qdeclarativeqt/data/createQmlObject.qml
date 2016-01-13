import QtQuick 1.0

Item {
    id: root

    // errors resulting in exceptions
    property QtObject incorrectArgCount1: Qt.createQmlObject()
    property QtObject incorrectArgCount2: Qt.createQmlObject("import QtQuick 1.0\nQtObject{}", root, "main.qml", 10)
    property QtObject noParent: Qt.createQmlObject("import QtQuick 1.0\nQtObject{\nproperty int test: 13}", 0)
    property QtObject notAvailable: Qt.createQmlObject("import QtQuick 1.0\nQtObject{Blah{}}", root)
    property QtObject errors: Qt.createQmlObject("import QtQuick 1.0\nQtObject{\nproperty int test: 13\nproperty int test: 13\n}", root, "main.qml")

    property bool emptyArg: false

    property bool success: false

    Component.onCompleted: {
        // errors resulting in nulls
        emptyArg = (Qt.createQmlObject("", root) == null);
        try {
            Qt.createQmlObject("import QtQuick 1.0\nQtObject{property int test\nonTestChanged: QtObject{}\n}", root)
        } catch (error) {
            console.log("RunTimeError: ",error.message);
        }

        var o = Qt.createQmlObject("import QtQuick 1.0\nQtObject{\nproperty int test: 13\n}", root);
        success = (o.test == 13);

        Qt.createQmlObject("import QtQuick 1.0\nItem {}\n", root);
    }
}
