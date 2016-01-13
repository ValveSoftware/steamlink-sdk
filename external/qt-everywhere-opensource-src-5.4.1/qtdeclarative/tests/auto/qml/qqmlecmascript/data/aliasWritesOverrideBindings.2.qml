import QtQuick 2.0

Item {
    id: me
    property bool test: false

    property int value: 9

    AliasBindingsOverrideTargetType {
        id: aliasType
    }

    Component.onCompleted: {
        if (aliasType.aliasProperty != 294) return;

        aliasType.data = 8;
        if (aliasType.aliasProperty != 336) return;

        aliasType.aliasProperty = 4;
        if (aliasType.aliasProperty != 4) return;

        aliasType.data = 7;
        if (aliasType.aliasProperty != 4) return;

        test = true;
    }
}


