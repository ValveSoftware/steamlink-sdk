import QtQuick 2.0
import "componentDir"

Item {
    id: root

    property FirstComponent first: FirstComponent { }
    property FirstComponent second
    property SecondComponent third: SecondComponent { }

    property list<FirstComponent> fclist: [
        FirstComponent {
            a: 15
        }
    ]
    property list<SecondComponent> sclistOne: [
        SecondComponent {
            a: "G'day, World"
        },
        SecondComponent { },
        SecondComponent {
            b: true
        }
    ]
    property list<SecondComponent> sclistTwo

    Component.onCompleted: {
        var c1 = Qt.createComponent("./componentDir/FirstComponent.qml");
        var o1 = c1.createObject(root);
        second = o1;
    }
}
