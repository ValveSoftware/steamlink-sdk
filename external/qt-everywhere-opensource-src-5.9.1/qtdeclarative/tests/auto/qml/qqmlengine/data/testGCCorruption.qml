import QtQml 2.0

QtObject {
    id: root
    property int count: 10000;
    property var items: [];

    property Component component: Component {
        id: component;
        QtObject {
        }
    }

    property int iterations: 0
    Component.onCompleted: {
        for (var iterations = 0; iterations < 5; ++iterations) {
            for (var i=0; i<items.length; ++i) {
                items[i].destroy();
            }

            for (var i=0; i<root.count; ++i) {
                var object = component.createObject();
                items[i] = object
            }
        }

        // if we crash, then something bad has happened. :)
    }
}
