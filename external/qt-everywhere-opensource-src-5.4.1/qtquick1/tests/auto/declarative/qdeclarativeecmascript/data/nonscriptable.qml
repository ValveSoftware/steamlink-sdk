import Qt.test 1.0
import QtQuick 1.0

MyQmlObject {
    id: root

    property bool readOk: false;
    property bool writeOk: false

    Component.onCompleted: {
        readOk = (root.nonscriptable == undefined);

        try {
            root.nonscriptable = 10
        } catch (e) {
            writeOk = true;
        }
    }
}
