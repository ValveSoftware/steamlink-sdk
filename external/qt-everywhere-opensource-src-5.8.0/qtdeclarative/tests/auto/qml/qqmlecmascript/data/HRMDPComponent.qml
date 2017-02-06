import QtQuick 2.0
import Qt.test.qobjectApi 1.0 as QObjectApi

Item {
    property int variantCanary: 5
    property var varCanary: 12

    Component.onCompleted: {
        QObjectApi.QObject.qobjectTestWritableProperty = 42;
    }

    Component.onDestruction: {
        QObjectApi.QObject.qobjectTestWritableProperty = 43;
    }
}
