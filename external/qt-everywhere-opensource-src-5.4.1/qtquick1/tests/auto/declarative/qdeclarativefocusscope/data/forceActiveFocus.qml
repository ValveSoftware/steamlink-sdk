import QtQuick 1.0

Rectangle {
    objectName: "root"
    FocusScope {
        objectName: "scope"
        Item {
            objectName: "item-a1"
            FocusScope {
                objectName: "scope-a"
                Item {
                    objectName: "item-a2"
                }
            }
        }
        Item {
            objectName: "item-b1"
            FocusScope {
                objectName: "scope-b"
                Item {
                    objectName: "item-b2"
                }
            }
        }
    }
}
