import QtQuick 2.0
import Qt.test.fallbackBindingsObject 1.0 as SingletonType

Item {
    property bool success: false
    property string foo: SingletonType.Fallback.test

    Component.onCompleted: success = (foo == '100')
}
