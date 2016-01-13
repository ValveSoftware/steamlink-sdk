import QtQuick 2.0

Item {
    id: root
    width: 100
    height: 100

    function createChild() {
        Qt.createQmlObject("import QtQuick 2.0;" +
                           "Item { width: parent.width; }", root);
    }
}
