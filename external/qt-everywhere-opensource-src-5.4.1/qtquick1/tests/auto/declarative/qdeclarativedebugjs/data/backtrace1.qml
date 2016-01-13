import QtQuick 1.0
import "backtrace1.js" as Script

Item {
    id: root

    property int result:0

    Component.onCompleted:
    {
        root.result = 10;
        Script.functionInScript(4,5);
        root.name = "nemo";
        Script.logger(root.simpleBinding);
    }

    //DO NOT CHANGE CODE ABOVE
    //ADD CODE FROM HERE

    property string name
    property int simpleBinding: result

    VisualItemModel {
        id: itemModel
        Rectangle { height: 30; width: 80; color: "red" }
        Rectangle { height: 30; width: 80; color: "green" }
        Rectangle { height: 30; width: 80; color: "blue" }
    }

    ListView {
        anchors.fill: parent
        model: itemModel

        Component.onCompleted:
        {
            Script.logger("List Loaded");
        }
    }

}

