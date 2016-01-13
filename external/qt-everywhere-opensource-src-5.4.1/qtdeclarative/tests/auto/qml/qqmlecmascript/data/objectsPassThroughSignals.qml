import QtQuick 2.0

QtObject {
    id: root

    property bool test: false

    signal mysignal(variant object);
    function myslot(object)
    {
        test = (object == root);
    }

    Component.onCompleted: {
        mysignal.connect(this, myslot);
        mysignal(root);
    }
}
