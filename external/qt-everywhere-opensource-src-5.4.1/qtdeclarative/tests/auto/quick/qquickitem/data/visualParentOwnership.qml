import QtQuick 2.0

Item {
    Component {
        id: factory
        Item {}
    }

    property Item keepAliveProperty;

    function createItemWithoutParent() {
        return factory.createObject(/*parent*/ null);
    }
}
