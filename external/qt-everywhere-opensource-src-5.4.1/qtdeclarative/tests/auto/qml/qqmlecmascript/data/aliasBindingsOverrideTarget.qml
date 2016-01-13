import QtQuick 2.0

Item {
    id: me
    property bool test: false

    property int value: 9

    AliasBindingsOverrideTargetType {
        id: aliasType
        aliasProperty: me.value
    }

    Component.onCompleted: {
        if (aliasType.aliasProperty != 9) return;

        me.value = 11;
        if (aliasType.aliasProperty != 11) return;

        aliasType.data = 8;
        if (aliasType.aliasProperty != 11) return;

        me.value = 4;
        if (aliasType.aliasProperty != 4) return;

        test = true;
    }
}
