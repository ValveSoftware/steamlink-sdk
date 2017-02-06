import Test 1.0
import QtQuick 2.0

BindingsSpliceCorrectlyType5 {
    property bool test: false

    property int dataProperty2: 8

    point: Qt.point(dataProperty2, dataProperty2);

    Component.onCompleted: {
        if (point.x != 8) return;
        if (point.y != 8) return;

        dataProperty = 9;

        if (point.x != 8) return;
        if (point.y != 8) return;

        dataProperty2 = 13;

        if (point.x != 13) return;
        if (point.y != 13) return;

        test = true;
    }
}
