import QtQuick 2.0

Item {
    width: 400
    height: 400
    Accessible.name: "root"

    // button, not checkable
    Rectangle {
        objectName: "button1"
        y: 20
        width: 100; height: 20
        Accessible.role : Accessible.Button
    }

    // button, checkable, not checked
    Rectangle {
        objectName: "button2"
        y: 40
        width: 100; height: 20
        Accessible.role : Accessible.Button
        Accessible.checkable: checkable
        Accessible.checked: checked
        property bool checkable: true
        property bool checked: false
    }

    // button, checkable, checked
    Rectangle {
        objectName: "button3"
        y: 60
        width: 100; height: 20
        Accessible.role : Accessible.Button
        Accessible.checkable: checkable
        Accessible.checked: checked
        property bool checkable: true
        property bool checked: true
    }

    // check box, checked
    Rectangle {
        objectName: "checkbox1"
        y: 80
        width: 100; height: 20
        Accessible.role : Accessible.CheckBox
        Accessible.checked: checked
        property bool checked: true
    }
    // check box, not checked
    Rectangle {
        objectName: "checkbox2"
        y: 100
        width: 100; height: 20
        Accessible.role : Accessible.CheckBox
        Accessible.checked: checked
        property bool checked: false
    }
}

