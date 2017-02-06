import QtQuick 2.0

Rectangle{
    id: topLevelItem
    height: 480
    width: 320

    property string imageSize: "72x96"

    ListModel{
        id: imageModel1
        ListElement{
            name: "red"
            size: "72x96"
            hex: "ff0000"
        }
        ListElement{
            name: "green"
            size: "72x96"
            hex:  "00ff00"
        }
        ListElement{
            name: "blue"
            size: "72x96"
            hex: "0000ff"
        }
        ListElement{
            name: "cyan"
            size: "72x96"
            hex: "00ffff"
        }
        ListElement{
            name: "orange"
            size: "72x96"
            hex:  "ffa500"
        }
        ListElement{
            name: "violet"
            size: "72x96"
            hex: "ee82ee"
        }
        ListElement{
            name: "yellow"
            size: "72x96"
            hex: "ffff00"
        }
    }
    ListModel{
        id: imageModel2
        ListElement{
            name: "blue"
            size: "72x96"
            hex: "0000ff"
        }
        ListElement{
            name: "cyan"
            size: "72x96"
            hex: "00ffff"
        }
        ListElement{
            name: "violet"
            size: "72x96"
            hex: "ee82ee"
        }
        ListElement{
            name: "orange"
            size: "72x96"
            hex:  "ffa500"
        }
        ListElement{
            name: "yellow"
            size: "72x96"
            hex: "ffff00"
        }ListElement{
            name: "green"
            size: "72x96"
            hex:  "00ff00"
        }
        ListElement{
            name: "red"
            size: "72x96"
            hex: "ff0000"
        }
    }
    Component{
        id: colorImageDelegate
        Column{
            Text{ font.family: "Arial"; font.pointSize: 8; text:  name +" "+size+" Hex: "+hex }
            Image{
                source: "../shared/"+name+"_"+imageSize+".png"
            }
        }
    }
    Rectangle{
        id: rect1
        x: 0
        y: 0
        width: 180
        height: 800
    }
    Rectangle{
        id: rect2
        x: 181
        y: 0
        width: 180
        height: 800
    }
    ListView{
        id: lv1
        x: 0
        y: 0
        property string image_size: imageSize
        model: imageModel1
        delegate: colorImageDelegate
        anchors.fill: rect1
    }
    ListView{
        id: lv2
        property string image_size: imageSize
        model: imageModel2
        delegate: colorImageDelegate
        anchors.fill: rect2
    }
}
