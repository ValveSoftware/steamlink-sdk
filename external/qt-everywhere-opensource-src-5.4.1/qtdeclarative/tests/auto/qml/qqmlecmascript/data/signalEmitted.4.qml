import QtQuick 2.0
import Qt.test 1.0 as ModApi

Item {
    id: root

    property bool success: false
    property bool c1HasBeenDestroyed: false

    SignalEmittedComponent {
        id: c2
        function c1aChangedHandler() {
            // this should still be called, after c1 has been destroyed by gc,
            // because the onDestruction handler of c1 will be triggered prior
            // to when c1 will be invalidated.
            if (root.c1HasBeenDestroyed)
                root.success = true;
        }
    }

    Component.onCompleted: {
        // dynamically construct sibling.  When it goes out of scope, it should be gc'd.
        // note that the gc() will call weakqobjectcallback which will set queued for
        // deletion flag -- thus QQmlData::wasDeleted() will return true for that object..
        var comp = Qt.createComponent("SignalEmittedComponent.qml", root);
        var c1 = comp.createObject(null); // JS ownership
        c1.onAChanged.connect(c2.c1aChangedHandler);
        c1HasBeenDestroyed = true; // gc will collect c1.
        // return to event loop.
    }

    function destroyC2() {
        // we must gc() c1 first, then destroy c2, then handle events.
        c2.destroy();
    }
}
