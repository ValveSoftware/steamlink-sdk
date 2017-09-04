import QtQuick 2.0

Item{
    id: root
    property QtObject declarativerectangle : null
    property QtObject declarativeitem : null

    property QtObject bindingTestObject : null
    property QtObject bindingThisTestObject : null

    Component{
        id: a
        Rectangle {
            property Rectangle innerRect: Rectangle { border.width: 20 }
        }
    }
    Component{
        id: b
        Item{
            property bool testBool: false
            property int testInt: null
            property QtObject testObject: null
        }
    }

    // test passing in bindings
    width: 100
    Component {
        id: c
        Item {
            property int testValue
            width: 300
        }
    }

    Component.onCompleted: {
        root.declarativerectangle = a.createObject(root, {"x":17,"y":17, "color":"white", "border.width":3, "innerRect.border.width": 20});
        root.declarativeitem = b.createObject(root, {"x":17,"y":17,"testBool":true,"testInt":17,"testObject":root});

        root.bindingTestObject = c.createObject(root, {'testValue': Qt.binding(function(){return width * 3}) })  // use root.width
        root.bindingThisTestObject = c.createObject(root, {'testValue': Qt.binding(function(){return this.width * 3}) })  // use width of Item within 'c'
    }
}
