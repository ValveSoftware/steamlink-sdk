import QtQuick 2.0

QtObject {
    id: root
    Component.onCompleted: { setObject(root); getObject() }
}
