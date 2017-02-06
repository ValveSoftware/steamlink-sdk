import QtQuick 2.0

import "Types.js" as Types

Rectangle {
    id: root
    property bool success: false

    color: "white"
    height: 100
    width: 100

    signal modelChanged

    Timer {
        id: timer
        interval: 100
        onTriggered: {
            root.modelChanged();
            root.success = true;
        }
    }

    Loader{
        id: weekPage
        sourceComponent: Component {
            Item{
                function createAllDayEvents() {
                    if (3 == Types.Foo) {
                        console.log("Hello")
                    }
                }
            }
        }
        onLoaded: root.modelChanged.connect(item.createAllDayEvents);
    }

    Component.onCompleted: {
        weekPage.sourceComponent = null
        timer.running = true
    }
}
