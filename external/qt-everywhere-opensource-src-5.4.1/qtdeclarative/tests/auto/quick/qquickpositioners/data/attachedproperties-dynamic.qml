import QtQuick 2.0

Rectangle
{
  width: 300
  height: 100

  Row {
    id: pos
    objectName: "pos"
    anchors.fill: parent

    Rectangle {
      objectName: "rect0"
      width: 100
      height: 100
      color: 'red'
      property int index: Positioner.index
      property bool firstItem: Positioner.isFirstItem
      property bool lastItem: Positioner.isLastItem
    }

    Rectangle {
      objectName: "rect1"
      width: 100
      height: 100
      color: 'green'
      property int index: Positioner.index
      property bool firstItem: Positioner.isFirstItem
      property bool lastItem: Positioner.isLastItem
    }

    property QtObject subRect;

    function createSubRect() {
      var component = Qt.createComponent("rectangleComponent.qml");
      subRect = component.createObject(pos, {});
    }

    function destroySubRect() {
      subRect.destroy();
    }
  }
}
