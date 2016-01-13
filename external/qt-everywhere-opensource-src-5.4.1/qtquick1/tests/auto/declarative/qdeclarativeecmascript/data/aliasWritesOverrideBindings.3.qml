import QtQuick 1.0

Item {
    id: me
    property bool test: false

    property int value: 9

    AliasBindingsOverrideTargetType {
        id: aliasType
        pointAliasProperty.x: 9
    }

    Component.onCompleted: {
        if (aliasType.pointAliasProperty.x != 9) return;

        aliasType.data = 8;
        if (aliasType.pointAliasProperty.x != 9) return;

        test = true;
    }
}

