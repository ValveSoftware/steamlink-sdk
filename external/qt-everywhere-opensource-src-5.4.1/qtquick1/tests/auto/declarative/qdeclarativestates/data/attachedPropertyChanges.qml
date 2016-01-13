import Qt.test 1.0
import QtQuick 1.0

Item {
    id: item
    width: 100; height: 100
    MyRectangle.foo: 0

    states: State {
        name: "foo1"
        PropertyChanges {
            target: item
            MyRectangle.foo: 1
            width: 50
        }
    }

    Component.onCompleted: item.state = "foo1"
}

