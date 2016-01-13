import QtQuick 2.0;

Rectangle {
  objectName: "rect2"
  color: "blue"
  width: 100
  height: 100
  property int index: Positioner.index
  property bool firstItem: Positioner.isFirstItem
  property bool lastItem: Positioner.isLastItem
}
