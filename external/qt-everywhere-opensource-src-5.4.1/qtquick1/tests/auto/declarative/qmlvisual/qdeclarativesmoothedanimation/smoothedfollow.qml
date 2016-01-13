import QtQuick 1.0

Rectangle {
    width: 400; height: 360; color: "gray"

    Rectangle {
        id: rect
        width: 25; height: 10; y: 15; color: "black"
        SequentialAnimation on x {
            loops: Animation.Infinite
            NumberAnimation { from: 25; to: 350; duration: 1000 }
            NumberAnimation { from: 350; to: 25; duration: 1000 }
        }
    }

    Rectangle {
        width: 25; height: 10; x: rect.x; y: 30; color: "red"
        Behavior on x { SmoothedAnimation { velocity: 200 } }
    }

    Rectangle {
        width: 25; height: 10; x: rect.x; y: 45; color: "yellow"
        Behavior on x { SmoothedAnimation { velocity: 150; reversingMode: SmoothedAnimation.Immediate } }
    }

    Rectangle {
        width: 25; height: 10; x: rect.x; y: 60; color: "green"
        Behavior on x { SmoothedAnimation { velocity: 100; reversingMode: SmoothedAnimation.Sync } }
    }

    Rectangle {
        width: 25; height: 10; x: rect.x; y: 75; color: "purple"
        Behavior on x { SmoothedAnimation { velocity: 100; maximumEasingTime: 100 } }
    }

    Rectangle {
        width: 25; height: 10; x: rect.x; y: 90; color: "blue"
        Behavior on x { SmoothedAnimation { velocity: -1; duration: 300 } }
    }

    //rect2 has jerky movement, but the rects following it should be smooth
    Rectangle {
        id: rect2
        property int dir: 1
        width: 25; height: 10; x:25; y: 120; color: "black"
        function advance(){
            if(x >= 350)
                dir = -1;
            if(x <= 25)
                dir = 1;
            x += 65.0 * dir;
        }
    }
    Timer{
        interval: 200
        running: true
        repeat: true
        onTriggered: rect2.advance();
    }

    Rectangle {
        width: 25; height: 10; x: rect2.x; y: 135; color: "red"
        Behavior on x { SmoothedAnimation { velocity: 200 } }
    }

    Rectangle {
        width: 25; height: 10; x: rect2.x; y: 150; color: "yellow"
        Behavior on x { SmoothedAnimation { velocity: 150; reversingMode: SmoothedAnimation.Immediate } }
    }

    Rectangle {
        width: 25; height: 10; x: rect2.x; y: 165; color: "green"
        Behavior on x { SmoothedAnimation { velocity: 100; reversingMode: SmoothedAnimation.Sync } }
    }

    Rectangle {
        width: 25; height: 10; x: rect2.x; y: 180; color: "purple"
        Behavior on x { SmoothedAnimation { velocity: 100; maximumEasingTime: 100 } }
    }

    Rectangle {
        width: 25; height: 10; x: rect2.x; y: 195; color: "blue"
        Behavior on x { SmoothedAnimation { velocity: -1; duration: 300 } }
    }

    //rect3 just jumps , but the rects following it should be smooth
    Rectangle {
        id: rect3
        width: 25; height: 10; x:25; y: 240; color: "black"
        function advance(){
            if(x == 25)
                x = 350;
            else
                x = 25;
        }
    }
    Timer{
        interval: 1000
        running: true
        repeat: true
        onTriggered: rect3.advance();
    }

    Rectangle {
        width: 25; height: 10; x: rect3.x; y: 255; color: "red"
        Behavior on x { SmoothedAnimation { velocity: 200 } }
    }

    Rectangle {
        width: 25; height: 10; x: rect3.x; y: 270; color: "yellow"
        Behavior on x { SmoothedAnimation { velocity: 150; reversingMode: SmoothedAnimation.Immediate } }
    }

    Rectangle {
        width: 25; height: 10; x: rect3.x; y: 285; color: "green"
        Behavior on x { SmoothedAnimation { velocity: 100; reversingMode: SmoothedAnimation.Sync } }
    }

    Rectangle {
        width: 25; height: 10; x: rect3.x; y: 300; color: "purple"
        Behavior on x { SmoothedAnimation { velocity: 100; maximumEasingTime: 100 } }
    }

    Rectangle {
        width: 25; height: 10; x: rect3.x; y: 315; color: "blue"
        Behavior on x { SmoothedAnimation { velocity: -1; duration: 300 } }
    }
}
