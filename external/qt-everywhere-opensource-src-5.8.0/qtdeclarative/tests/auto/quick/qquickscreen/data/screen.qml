import QtQuick 2.0
import QtQuick.Window 2.0 as Window

Item {
    width: 100
    height: 100
    property int w: Window.Screen.width
    property int h: Window.Screen.height
    property int curOrientation: Window.Screen.orientation
    property int priOrientation: Window.Screen.primaryOrientation
    property int updateMask: Window.Screen.orientationUpdateMask
    property real devicePixelRatio: Window.Screen.devicePixelRatio

    Window.Screen.orientationUpdateMask: Qt.LandscapeOrientation | Qt.InvertedLandscapeOrientation
}
