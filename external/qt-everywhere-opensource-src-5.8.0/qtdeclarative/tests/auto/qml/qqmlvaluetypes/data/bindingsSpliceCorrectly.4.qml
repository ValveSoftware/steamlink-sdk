import Test 1.0
import QtQuick 2.0

BindingsSpliceCorrectlyType4 {
    property bool test: false

    property int dataProperty2: 8

    point.x: dataProperty2

    Component.onCompleted: {
        if (point.x != 8) return;
        if (point.y != 4) return;

        dataProperty = 9;

        if (point.x != 8) return;
        if (point.y != 4) return;

        dataProperty2 = 13;

        if (point.x != 13) return;
        if (point.y != 4) return;

        test = true;
    }
}
