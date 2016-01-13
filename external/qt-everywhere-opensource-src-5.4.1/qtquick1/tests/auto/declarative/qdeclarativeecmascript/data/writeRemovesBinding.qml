import QtQuick 1.0

QtObject {
    id: root

    property bool test: false

    property real data: 9
    property real binding: data

    property alias aliasProperty: root.aliasBinding
    property real aliasBinding: data

    Component.onCompleted: {
        // Non-aliased properties
        if (binding != 9) return;

        data = 11;
        if (binding != 11) return;

        binding = 6;
        if (binding != 6) return;

        data = 3;
        if (binding != 6) return;


        // Writing through an aliased property
        if (aliasProperty != 3) return;
        if (aliasBinding != 3) return;

        data = 4;
        if (aliasProperty != 4) return;
        if (aliasBinding != 4) return;

        aliasProperty = 19;
        if (aliasProperty != 19) return;
        if (aliasBinding != 19) return;

        data = 5;
        if (aliasProperty != 19) return;
        if (aliasBinding != 19) return;

        test = true;
    }
}
