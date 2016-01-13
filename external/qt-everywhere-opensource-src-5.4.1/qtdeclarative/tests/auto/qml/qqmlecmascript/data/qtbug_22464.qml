import QtQuick 2.0

QtObject {
    property alias value: inner.value
    property bool test: false

    property variant dummy: QtObject {
        id: inner
        property variant value: Qt.rgba(1, 1, 0, 1);
    }

    Component.onCompleted: {
        test = (value == Qt.rgba(1, 1, 0, 1));
    }
}
