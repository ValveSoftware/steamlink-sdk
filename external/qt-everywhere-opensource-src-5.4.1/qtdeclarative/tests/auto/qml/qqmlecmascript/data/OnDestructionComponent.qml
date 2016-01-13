import QtQuick 2.0
import Qt.test 1.0 as ModApi

Item {
    id: sec

    property int a: 10
    Component.onDestruction: ModApi.QObject.setSpecificProperty(sec, "a", 20);
}
