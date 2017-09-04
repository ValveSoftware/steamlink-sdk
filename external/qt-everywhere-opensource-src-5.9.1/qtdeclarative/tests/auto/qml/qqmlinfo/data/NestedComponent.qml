import QtQuick 2.0

QtObject {
    property variant nested
    property variant nested2: nested.nested

    property variant component
    component: Component {
        id: myComponent
        NestedObject { property string testProp: "test" }
    }

    property variant component2
    component2: Component {
        id: myComponent2
        Image { property string testProp: "test" }
    }

    Component.onCompleted: {
        nested = myComponent.createObject(0);
        nested2 = myComponent2.createObject(0);
    }
}
