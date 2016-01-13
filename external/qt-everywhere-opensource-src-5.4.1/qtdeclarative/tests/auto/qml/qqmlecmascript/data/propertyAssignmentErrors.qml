import QtQuick 2.0

QtObject {
    id: root

    property int a
    property variant b

    property bool test1: false;
    property bool test2: false;

    Component.onCompleted: {
        try {
            root.a = undefined;
        } catch(e) {
            if (e.message == "Cannot assign [undefined] to int" &&
                e.stack.indexOf("propertyAssignmentErrors.qml:14") != -1)
                root.test1 = true;
        }

        try {
            root.a = "Hello";
        } catch(e) {
            if (e.message == "Cannot assign QString to int" &&
                e.stack.indexOf("propertyAssignmentErrors.qml:22") != -1)
                root.test2 = true;
        }
    }
}
