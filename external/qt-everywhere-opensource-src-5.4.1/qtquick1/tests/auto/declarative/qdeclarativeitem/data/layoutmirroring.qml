import QtQuick 1.1

Item {
    property bool childrenInherit: true
    Item {
        objectName: "mirrored1"
        LayoutMirroring.enabled: true
        LayoutMirroring.childrenInherit: parent.childrenInherit
        Item {
            Item {
                objectName: "notMirrored1"
                LayoutMirroring.enabled: false
                Item {
                    objectName: "inheritedMirror1"
                }
            }
            Item {
                objectName: "inheritedMirror2"
            }
        }
    }
    Item {
        objectName: "mirrored2"
        LayoutMirroring.enabled: true
        LayoutMirroring.childrenInherit: false
        Item {
            objectName: "notMirrored2"
        }
    }
    Item {
        LayoutMirroring.enabled: true
        LayoutMirroring.childrenInherit: true
        Loader {
            id: loader
        }
    }
    states: State {
        name: "newContent"
        PropertyChanges {
            target: loader
            sourceComponent: component
        }
    }
    Component {
        id: component
        Item {
            objectName: "notMirrored3"
            LayoutMirroring.enabled: false
            Item {
                objectName: "inheritedMirror3"
            }
        }
    }
}
