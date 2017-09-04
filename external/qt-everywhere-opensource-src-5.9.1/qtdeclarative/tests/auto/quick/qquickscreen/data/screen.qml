import QtQuick 2.0
import QtQuick.Window 2.3 as Window

Item {
    width: 100
    height: 100
    property int w: Window.Screen.width
    property int h: Window.Screen.height
    property int curOrientation: Window.Screen.orientation
    property int priOrientation: Window.Screen.primaryOrientation
    property int updateMask: Window.Screen.orientationUpdateMask
    property real devicePixelRatio: Window.Screen.devicePixelRatio
    property int vx: Window.Screen.virtualX
    property int vy: Window.Screen.virtualY

    Window.Screen.orientationUpdateMask: Qt.LandscapeOrientation | Qt.InvertedLandscapeOrientation

    property int screenCount: Qt.application.screens.length

    property variant allScreens
    Component.onCompleted: {
        allScreens = [];
        var s = Qt.application.screens;
        for (var i = 0; i < s.length; ++i)
            allScreens.push(s[i]);
    }
}
