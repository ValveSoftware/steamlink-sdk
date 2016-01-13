import QtQuick 2.0

Rectangle {
     width: 400; height: 400; color: "blue"

    Rectangle {
        id: rect1
        color: "red"
        width: 60; height: 60;
        x: 100; y: 100;
        SmoothedAnimation on x { to: 200; velocity: 500 }
        SmoothedAnimation on y { to: 200; velocity: 500 }
    }

     Rectangle {
         objectName: "theRect"
         color: "green"
         width: 60; height: 60;
         x: rect1.x; y: rect1.y;
         // id are needed for SmoothedAnimation in order to avoid deferred creation
         Behavior on x { SmoothedAnimation { id: anim1; objectName: "easeX"; velocity: 400 } }
         Behavior on y { SmoothedAnimation { id: anim2; objectName: "easeY"; velocity: 400 } }
     }
 }
