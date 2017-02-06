import QtQuick 2.0

Item {
    property bool success: false

    BaseComponent {
        id: foo
    }

    Component.onCompleted: success = (foo.bar == '100')
}
