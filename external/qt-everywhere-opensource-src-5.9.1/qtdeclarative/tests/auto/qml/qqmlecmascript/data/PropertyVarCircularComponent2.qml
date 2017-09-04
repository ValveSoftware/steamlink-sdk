import QtQuick 2.0
import Qt.test 1.0

// Similar to PVCC.qml except that it has another var property
// It will have a different metaobject.
Item {
    id: first
    property var anotherVp: 6
    property var vp: Item {
        id: second
        property var vp: Item {
            id: third
            property var vp: Item {
                id: fourth
                property var vp: Item {
                    id: fifth
                    property int fifthCanary: 5
                    property var circ: third.vp
                    property MyScarceResourceObject srp;
                    srp: MyScarceResourceObject { id: scarceResourceProvider }
                    property variant memoryHog2: scarceResourceProvider.newScarceResource()
                }
            }
        }
    }
}
