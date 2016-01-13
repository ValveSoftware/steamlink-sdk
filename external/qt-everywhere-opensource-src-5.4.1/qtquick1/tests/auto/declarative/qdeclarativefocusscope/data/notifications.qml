import QtQuick 1.1

Item {
    objectName: "rootItem"

    property bool handlerFocus
    property bool handlerActiveFocus

    onFocusChanged: handlerFocus = focus
    onActiveFocusChanged: handlerActiveFocus = activeFocus

    Item {
        objectName: "item1"

        property bool handlerFocus
        property bool handlerActiveFocus

        onFocusChanged: handlerFocus = focus
        onActiveFocusChanged: handlerActiveFocus = activeFocus
    }

    Item {
        objectName: "item2"

        property bool handlerFocus
        property bool handlerActiveFocus

        onFocusChanged: handlerFocus = focus
        onActiveFocusChanged: handlerActiveFocus = activeFocus

        Item {
            objectName: "item3"

            property bool handlerFocus
            property bool handlerActiveFocus

            onFocusChanged: handlerFocus = focus
            onActiveFocusChanged: handlerActiveFocus = activeFocus
        }
    }

    FocusScope {
        objectName: "scope1"

        property bool handlerFocus
        property bool handlerActiveFocus

        onFocusChanged: handlerFocus = focus
        onActiveFocusChanged: handlerActiveFocus = activeFocus

        Item {
            objectName: "item4"

            property bool handlerFocus
            property bool handlerActiveFocus

            onFocusChanged: handlerFocus = focus
            onActiveFocusChanged: handlerActiveFocus = activeFocus
        }

        FocusScope {
            objectName: "scope2"

            property bool handlerFocus
            property bool handlerActiveFocus

            onFocusChanged: handlerFocus = focus
            onActiveFocusChanged: handlerActiveFocus = activeFocus

            Item {
                objectName: "item5"

                property bool handlerFocus
                property bool handlerActiveFocus

                onFocusChanged: handlerFocus = focus
                onActiveFocusChanged: handlerActiveFocus = activeFocus
            }
        }
    }
}
