import QtQuick.Window 2.0
import QtQuick 2.0

Window
{
    Rectangle {
        width: 10
        height: 10
        color: "blue"
    }

    onFrameSwapped: console.log("tick");
}
