import QtQuick 2.0

Rectangle {
  id: container;
  width: 600;
  height: 600;

  Image {
    id: image1
    source: "http://blog.qt.digia.com/wp-content/uploads/2013/07/Qt-team-Oslo1.jpeg"
    anchors.right: image2.left
  }

  Image  {
    id: image2
    source: "http://blog.qt.digia.com/wp-content/uploads/2013/07/Qt-team-Oslo1.jpeg"
    anchors.left: image1.right
    anchors.leftMargin: 20
  }
}
