import QtQuick 2.0
import QtQml.Models 2.1

Rectangle {
    id: root
    width: 240
    height: 320
    color: "#ffffff"


    Component {
        id: myDelegate
        Rectangle {
            id: wrapper
            objectName: "wrapper"
            height: 20
            width: 240
            Text {
                objectName: "delegateText"
                text: display
            }
            color: ListView.isCurrentItem ? "lightsteelblue" : "white"
        }
    }

    DelegateModel {
        id: delegateModel
        objectName: "delegateModel"
        model: testModel
        delegate: myDelegate
    }

    ListView {
        id: list
        objectName: "listView"
        model: delegateModel;
        focus: true
        anchors.fill: parent


        section {
            property: "SortRole";
            delegate: Rectangle {
                width: parent.width;
                height: 20;
                color: "steelblue";

                Text {
                    anchors {
                        fill: parent;
                        margins: 5;
                    }
                    text: section;
                }
            }
        }
    }
}
