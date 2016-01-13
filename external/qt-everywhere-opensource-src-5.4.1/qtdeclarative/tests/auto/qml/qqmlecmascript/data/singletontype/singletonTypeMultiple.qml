import QtQuick 2.0
import Qt.test.qobjectApis 1.0

Item {
    property int first: One.qobjectTestWritableProperty
    property int second: Two.twoTestProperty

    Component.onCompleted: {
        One.qobjectTestWritableProperty = 35;
    }
}
