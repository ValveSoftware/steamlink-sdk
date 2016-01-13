import QtQuick 2.0

Item{
    id: root

    property bool test1: false
    property bool test2: false

    property var i

    Component{
        id: component
        Item {
            property int dummy: 13
            property int dummy2: 26
        }
    }

    Component.onCompleted: {
        i = component.incubateObject(null, { dummy2: 19 });

        if (i.status != Component.Loading) return;
        if (i.object != null) return;

        i.onStatusChanged = function(status) {
            if (status != Component.Ready) return;
            if (i.object == null) return;
            if (i.object.dummy != 13) return;
            if (i.object.dummy2 != 19) return;
            test2 = true;
        }

        test1 = true;
    }
}

