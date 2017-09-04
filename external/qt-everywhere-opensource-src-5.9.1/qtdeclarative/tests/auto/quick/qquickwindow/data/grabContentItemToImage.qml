import QtQuick 2.0
import QtQuick.Window 2.2 as Window

Window.Window {
    visible: true
    width: 100
    height: 100
    property int success: 0

    function grabContentItemToImage() {
        contentItem.grabToImage(function (image) {
            success = 1
        })
    }
}
