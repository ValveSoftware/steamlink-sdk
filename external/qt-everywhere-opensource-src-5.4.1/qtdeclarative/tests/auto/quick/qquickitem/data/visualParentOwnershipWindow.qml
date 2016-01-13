import QtQuick 2.0
import QtQuick.Window 2.1

Window {
    Component {
        id: factory
        Item {}
    }

    property Item keepAliveProperty;

    function createItemWithoutParent() {
        return factory.createObject(/*parent*/ null);
    }
}
