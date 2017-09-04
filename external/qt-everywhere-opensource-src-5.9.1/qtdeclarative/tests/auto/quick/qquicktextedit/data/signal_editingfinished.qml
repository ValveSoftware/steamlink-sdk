import QtQuick 2.6

Item {
    property QtObject input1: input1
    property QtObject input2: input2

    width: 800; height: 600

    Column{
        TextEdit { id: input1; activeFocusOnTab: true }
        TextEdit { id: input2; activeFocusOnTab: true }
    }
}
