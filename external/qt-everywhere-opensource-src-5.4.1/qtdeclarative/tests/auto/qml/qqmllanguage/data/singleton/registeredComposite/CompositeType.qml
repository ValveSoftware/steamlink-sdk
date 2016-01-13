import QtQuick 2.0
pragma Singleton

Item {
     id: singletonId

     property int testProp1: 325
     property int testProp2: 225
     property int testProp3: 155

     width: 25; height: 25

     Rectangle {
         id: rectangle
         border.color: "white"
         anchors.fill: parent
     }
}

