import QtQuick 1.0

/*
  Test nested MouseArea with different drag axes.
*/

Rectangle{
    width:400
    height:360
    color: "white"
    Flickable {
        anchors.fill: parent
        contentWidth: 600
        contentHeight: 600
        Rectangle{
            id: draggable
            width:200; height:200; color: "lightsteelblue"
            opacity: ma1.drag.active ? 0.5 : 1.0
            y:20
            MouseArea{
                id: ma1
                objectName: "one"
                anchors.fill: parent
                drag.target: draggable
                drag.axis: "XandYAxis"
                drag.filterChildren: true
                drag.minimumX: 0
                drag.maximumX: 200
                drag.minimumY: 20
                drag.maximumY: 220
                Rectangle{
                    id: draggable_inner
                    width:40; height:40; color: "red"
                        y:20
                    MouseArea{
                        objectName: "two"
                        anchors.fill: parent
                        drag.target: draggable_inner
                        drag.axis: "XAxis"
                        drag.minimumX: 0
                        drag.maximumX: 360
                    }
                }
            }
        }
        Rectangle{
            id: draggable3
            width:40; height:40; color: "green"
            opacity: ma3.drag.active ? 0.5 : 1.0
            y:210
            MouseArea{
                id: ma3
                objectName: "three"
                anchors.fill: parent
                drag.target: draggable3
                drag.axis: "XAxis"
                drag.minimumX: 0
                drag.maximumX: 360
            }
        }
    }
}
