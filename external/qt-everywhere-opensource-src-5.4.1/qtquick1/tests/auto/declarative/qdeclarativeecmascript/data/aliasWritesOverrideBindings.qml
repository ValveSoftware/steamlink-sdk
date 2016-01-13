import QtQuick 1.0

Item {
    id: me
    property bool test: false

    property int value: 9

    AliasBindingsOverrideTargetType {
        id: aliasType
        aliasProperty: 11
    }

    Component.onCompleted: {
        if (aliasType.aliasProperty != 11) return;

        aliasType.data = 8;
        if (aliasType.aliasProperty != 11) return;

        test = true;
    }
}

