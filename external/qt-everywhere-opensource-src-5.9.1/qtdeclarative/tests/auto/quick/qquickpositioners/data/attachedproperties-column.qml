import QtQuick 2.0

Rectangle {
  width: 100
  height: 200

  Column {

    Rectangle {
      width: 100
      height: 100
      color: 'red'
      visible: false
    }

    Rectangle {
      objectName: "greenRect"
      width: 100
      height: 100
      color: 'green'
      property int posIndex: Positioner.index
      property bool isFirstItem: Positioner.isFirstItem
      property bool isLastItem: Positioner.isLastItem
    }

    Rectangle {
      width: 100
      height: 100
      color: 'blue'
      visible: false
    }

    Rectangle {
      objectName: "yellowRect"
      width: 100
      height: 100
      color: 'yellow'

      property int posIndex: -1
      property bool isFirstItem: false
      property bool isLastItem: false

      function onDemandPositioner() {
        posIndex = Positioner.index;
        isFirstItem = Positioner.isFirstItem
        isLastItem = Positioner.isLastItem
      }
    }
  }
}
