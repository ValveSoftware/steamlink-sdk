import QtQuick 2.0

QtObject {
    objectName: "Joe"
    id: me
    property alias test: me.objectName
}
