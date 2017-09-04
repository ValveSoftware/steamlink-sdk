import QtQuick 2.3
import QtQuick.Window 2.2

Window {
    id: root
    property var transientWindow
    property Loader loader1: Loader {
        sourceComponent: Item {
            Loader {
                id: loader2
                sourceComponent : Window {
                    id: inner
                    visible: true
                    Component.onCompleted: root.transientWindow = inner
                }
            }
            Component.onDestruction: {
                loader2.active = false;
            }
        }
    }
}
