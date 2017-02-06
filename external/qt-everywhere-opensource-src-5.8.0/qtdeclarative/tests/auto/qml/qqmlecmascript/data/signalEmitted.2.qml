import QtQuick 2.0
import Qt.test 1.0 as ModApi

Item {
    id: root

    property bool success: false
    property bool c1HasBeenDestroyed: false

    SignalEmittedComponent {
        id: c1
    }

    SignalEmittedComponent {
        id: c2
        property int c1a: if (c1) c1.a; else 0; // will change during onDestruction handler of c1.
        onC1aChanged: {
            // this should still be called, after c1 has been destroyed.
            if (root.c1HasBeenDestroyed && c1a == 20) c1.setSuccessPropertyOf(root, true);
        }
    }

    Component.onCompleted: {
        // destroy c1 directly
        c1HasBeenDestroyed = true;
        c1.destroy();
        // return to event loop.
    }
}
