import QtQuick 2.0
pragma Singleton

Item {
     id: singletonId

     property int testProp1: 125
     property int testProp2: 25
     property int testProp3: -55

     width: 25; height: 25

     Rectangle {
         id: rectangle
         border.color: "white"
         anchors.fill: parent
     }
}