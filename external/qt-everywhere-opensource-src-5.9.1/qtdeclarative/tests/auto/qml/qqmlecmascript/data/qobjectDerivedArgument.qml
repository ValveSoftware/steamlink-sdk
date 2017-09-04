import QtQuick 2.0
import Qt.test 1.0

MyQmlObject {
    id: root
    stringProperty: 'hello'
    property var child

    property bool result: false

    Component.onCompleted: {
        child = invokable.createMyQmlObject('goodbye');

        result = (invokable.getStringProperty(root) == 'hello') &&
                 (invokable.getStringProperty(child) == 'goodbye');
    }
}
