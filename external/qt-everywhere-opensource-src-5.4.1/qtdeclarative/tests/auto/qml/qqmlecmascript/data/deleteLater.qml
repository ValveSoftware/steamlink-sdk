import QtQuick 2.0

QtObject {
    id: root
    property bool test: false

    Component.onCompleted: {
        try {
            root.deleteLater()
        } catch(e) {
            test = true;
        }
    }
}
