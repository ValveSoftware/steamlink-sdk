import QtQuick 1.0

Item{
    id: root
    property QtObject qobject : null
    property QtObject declarativeitem : null
    property QtObject graphicswidget: null
    Component{id: a; QtObject{} }
    Component{id: b; Item{} }
    Component{id: c; QGraphicsWidget{} }
    Component.onCompleted: {
        root.qobject = a.createObject(root);
        root.declarativeitem = b.createObject(root);
        root.graphicswidget = c.createObject(root);
    }
}
