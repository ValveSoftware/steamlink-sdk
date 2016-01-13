import QtQuick 1.0

Rectangle {
  id: container;
  width: 600;
  height: 600;

  Image {
    id: image1
    source: "http://labs.trolltech.com/blogs/wp-content/uploads/2009/03/3311388091_ac2a257feb.jpg"
    anchors.right: image2.left
  }

  Image  {
    id: image2
    source: "http://labs.trolltech.com/blogs/wp-content/uploads/2009/03/oslo_groupphoto.jpg"
    anchors.left: image1.right
    anchors.leftMargin: 20
  }
}
