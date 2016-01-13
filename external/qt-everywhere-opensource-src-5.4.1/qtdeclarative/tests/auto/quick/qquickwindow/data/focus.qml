import QtQuick 2.0
import QtQuick.Window 2.0 as Window

Window.Window {

    width: 400
    height: 300

     Item {
          objectName: "item1"
     }
     Item {
          objectName: "item2"
     }

     FocusScope {
         Item { objectName: "item3" }
     }
}
