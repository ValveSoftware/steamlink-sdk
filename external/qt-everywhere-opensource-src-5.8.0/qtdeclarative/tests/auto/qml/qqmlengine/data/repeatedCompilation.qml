import QtQuick 2.0

VMEExtendVMEComponent {
    property bool success: false

    Component.onCompleted: {
        success = (foo == 'bar') && (bar == 'baz')
    }
}
