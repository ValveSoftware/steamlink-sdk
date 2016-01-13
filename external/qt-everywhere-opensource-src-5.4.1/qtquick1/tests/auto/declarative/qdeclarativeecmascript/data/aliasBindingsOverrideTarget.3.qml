import QtQuick 1.0

Item {
    id: root
    property bool test: false;

    property int value1: 10
    property int value2: 11

    AliasBindingsOverrideTargetType3 {
        id: obj

        testProperty: root.value1 * 9
        aliasProperty: root.value2 * 10
    }

    Component.onCompleted: {
        if (obj.testProperty != 110) return;
        if (obj.aliasProperty != 110) return;

        test = true;
    }
}

