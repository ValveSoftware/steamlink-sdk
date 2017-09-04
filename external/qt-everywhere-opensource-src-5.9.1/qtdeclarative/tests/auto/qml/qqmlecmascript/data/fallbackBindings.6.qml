import QtQuick 2.0
import Qt.test.fallbackBindingsDerived 1.0

Item {
    property bool success: false
    property string foo: FallbackBindingsType.test

    Component.onCompleted: success = (foo == 'hello')
}
