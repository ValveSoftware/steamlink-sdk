import QtQuick 2.0

Item{
    id: root
    property QtObject qobject : null
    property QtObject declarativeitem : null
    Component{id: a; QtObject{} }
    Component{id: b; Item{} }
    Component.onCompleted: {
        root.qobject = a.createObject(root);
        root.declarativeitem = b.createObject(root);
    }
}
