import QtQuick 2.0

Item {
    id: me
    property bool test: false

    property int value: 9

    AliasBindingsOverrideTargetType {
        id: aliasType
        pointAliasProperty.x: me.value
    }

    Component.onCompleted: {
        if (aliasType.pointAliasProperty.x != 9) return;

        me.value = 11;
        if (aliasType.pointAliasProperty.x != 11) return;

        aliasType.data = 8;
        if (aliasType.pointAliasProperty.x != 11) return;

        me.value = 4;
        if (aliasType.pointAliasProperty.x != 4) return;

        test = true;
    }
}

