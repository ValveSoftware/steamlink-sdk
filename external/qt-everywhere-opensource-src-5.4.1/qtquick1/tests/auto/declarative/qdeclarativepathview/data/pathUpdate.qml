import QtQuick 1.0

Rectangle {
   width: 400
   height: 400

   PathView {
       id: view
       objectName: "pathView"
       anchors.fill: parent
       model: 10
       delegate: Rectangle { objectName: "wrapper"; color: "green"; width: 100; height: 100 }
       path: Path {
           startX: view.width/2; startY: 0
           PathLine { x: view.width/2; y: view.height }
       }
   }
}
