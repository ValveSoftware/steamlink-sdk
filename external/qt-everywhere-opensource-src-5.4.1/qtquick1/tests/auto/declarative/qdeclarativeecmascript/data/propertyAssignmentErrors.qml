import QtQuick 1.0

QtObject {
    id: root

    property int a
    property variant b

    Component.onCompleted: {
        try {
            root.a = undefined;
        } catch(e) {
            console.log (e.fileName + ":" + e.lineNumber + ":" + e);
        }

        try {
            root.a = "Hello";
        } catch(e) {
            console.log (e.fileName + ":" + e.lineNumber + ":" + e);
        }
    }
}
