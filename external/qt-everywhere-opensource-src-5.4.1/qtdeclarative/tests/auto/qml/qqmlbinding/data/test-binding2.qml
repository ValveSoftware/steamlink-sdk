import QtQuick 2.0

Rectangle {
    id: screen
    width: 320; height: 240
    property string text
    property bool changeColor: false

    Text { id: s1; text: "Hello" }
    Rectangle { id: r1; width: 1; height: 1; color: "yellow" }
    Rectangle { id: r2; width: 1; height: 1; color: "red" }

    Binding { target: screen; property: "text"; value: s1.text }
    Binding { target: screen; property: "color"; value: r1.color }
    Binding { target: screen; property: "color"; value: r2.color; when: screen.changeColor == true }
}
