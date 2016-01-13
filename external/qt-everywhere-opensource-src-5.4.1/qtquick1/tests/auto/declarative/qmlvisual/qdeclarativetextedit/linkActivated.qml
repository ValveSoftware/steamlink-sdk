import QtQuick 1.1

Rectangle{
    width: 100
    height: 100
    TextEdit{
        text: '<a href="clicked">Click Me</a> '
        onLinkActivated: txt.text=link;
    }
    Text{
        id: txt
        y:50
        text: "unknown"
    }
}
