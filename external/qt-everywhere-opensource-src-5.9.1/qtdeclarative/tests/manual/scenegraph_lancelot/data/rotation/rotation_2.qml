import QtQuick 2.0

Rectangle {
    width: 320
    height: 480
    property int standardWidth: 60
    property int standardHeight: 60
    property int standardSpacing: 20
    property bool smoothing: true
    Grid{
        id: grid_0000
        anchors.top: parent.baseline
        anchors.left: parent.left
        columns: 4
        spacing: standardSpacing
        Rectangle{ color: "red"; width: standardWidth; height: standardHeight; transform: Rotation { origin.x: standardWidth/2; origin.y: standardHeight/2 ; axis{x: 0; y: 0; z:1} angle: 5; } smooth: smoothing}
        Rectangle{ color: "orange"; width: standardWidth; height: standardHeight; transform: Rotation { origin.x: 0; origin.y: 0 ; axis{x: 0; y: 0; z:1} angle: 10; } smooth: smoothing }
        Rectangle{ color: "yellow"; width: standardWidth; height: standardHeight; transform: Rotation { origin.x: 0; origin.y: 0 ; axis{x: 0; y: 0; z:1} angle: 15; } smooth: smoothing }
        Rectangle{ color: "blue"; width: standardWidth; height: standardHeight; transform: Rotation { origin.x: 0; origin.y: 0 ; axis{x: 0; y: 0; z:1} angle: 20; } smooth: smoothing }
        Rectangle{ color: "green"; width: standardWidth; height: standardHeight; transform: Rotation { origin.x: standardWidth/2; origin.y: standardWidth/2 ; axis{x: 0; y: 0; z:1} angle: 15; } smooth: smoothing}
        Rectangle{ color: "indigo"; width: standardWidth; height: standardHeight; transform: Rotation { origin.x: 0; origin.y: 0 ; axis{x: 0; y: 0; z:1} angle: 30; } smooth: smoothing}
        Rectangle{ color: "violet"; width: standardWidth; height: standardHeight; transform: Rotation { origin.x: 0; origin.y: 0 ; axis{x: 0; y: 0; z:1} angle: 35; } smooth: smoothing }
        Rectangle{ color: "light green"; width: standardWidth; height: standardHeight; transform: Rotation { origin.x: 0; origin.y: 0 ; axis{x: 0; y: 0; z:1} angle: 40; } smooth: smoothing }
    }
    Item{
        id: spacer_0000
        width: standardWidth
        height: standardHeight
        anchors.top: grid_0000.bottom
        anchors.left: grid_0000.left
    }
    Grid{
        id: grid_0001
        anchors.top: spacer_0000.bottom
        anchors.left: spacer_0000.left
        columns: 4
        spacing: standardSpacing
        Rectangle{ color: "#ff0000"; width: standardWidth; height: standardHeight; transform: Rotation { origin.z: 0; origin.y: 0 ; axis{x: 1; y: 0; z:0} angle: 5; } smooth: smoothing}
        Rectangle{ color: "#ff3333"; width: standardWidth; height: standardHeight; transform: Rotation { origin.z: 0; origin.y: 0 ; axis{x: 1; y: 0; z:0} angle: 20; } smooth: smoothing }
        Rectangle{ color: "#ff6666"; width: standardWidth; height: standardHeight; transform: Rotation { origin.z: 0; origin.y: 0 ; axis{x: 1; y: 0; z:0} angle: 30; } smooth: smoothing }
        Rectangle{ color: "#ff9999"; width: standardWidth; height: standardHeight; transform: Rotation { origin.z: 0; origin.y: 0 ; axis{x: 1; y: 0; z:0} angle: 40; } smooth: smoothing }
        Rectangle{ color: "#ffcccc"; width: standardWidth; height: standardHeight; transform: Rotation { origin.z: 0; origin.y: 0 ; axis{x: 1; y: 0; z:0} angle: 50; } smooth: smoothing }
        Rectangle{ color: "#ffeeff"; width: standardWidth; height: standardHeight; transform: Rotation { origin.z: 0; origin.y: 0 ; axis{x: 1; y: 0; z:0} angle: 60; } smooth: smoothing }
        Rectangle{ color: "#ccffff"; width: standardWidth; height: standardHeight; transform: Rotation { origin.z: 0; origin.y: 0 ; axis{x: 1; y: 0; z:0} angle: 70; } smooth: smoothing }
        Rectangle{ color: "#99ffff"; width: standardWidth; height: standardHeight; transform: Rotation { origin.z: 0; origin.y: 0 ; axis{x: 1; y: 0; z:0} angle: 80; } smooth: smoothing }
    }
    Item{
        id: spacer_0001
        width: standardWidth
        height: standardHeight
        anchors.top: grid_0001.bottom
        anchors.left: grid_0001.left
    }
    Grid{
        id: grid_0002
        anchors.top: spacer_0001.bottom
        anchors.left: spacer_0001.left
        columns: 4
        spacing: standardSpacing
        Rectangle{ color: "#ff0000"; width: standardWidth; height: standardHeight;  transform: Rotation { origin.z: 0; origin.y: 0 ; axis{x: 0; y: 1; z:0} angle: 10; } smooth: smoothing}
        Rectangle{ color: "#ff3333"; width: standardWidth; height: standardHeight;  transform: Rotation { origin.z: 0; origin.y: 0 ; axis{x: 0; y: 1; z:0} angle: 20; } smooth: smoothing}
        Rectangle{ color: "#ff6666"; width: standardWidth; height: standardHeight;  transform: Rotation { origin.z: 0; origin.y: 0 ; axis{x: 0; y: 1; z:0} angle: 30; } smooth: smoothing}
        Rectangle{ color: "#ff9999"; width: standardWidth; height: standardHeight;  transform: Rotation { origin.z: 0; origin.y: 0 ; axis{x: 0; y: 1; z:0} angle: 40; } smooth: smoothing}
        Rectangle{ color: "#ffcccc"; width: standardWidth; height: standardHeight;  transform: Rotation { origin.z: 0; origin.y: 0 ; axis{x: 0; y: 1; z:0} angle: 50; } smooth: smoothing}
        Rectangle{ color: "#ffeeff"; width: standardWidth; height: standardHeight;  transform: Rotation { origin.z: 0; origin.y: 0 ; axis{x: 0; y: 1; z:0} angle: 60; } smooth: smoothing}
        Rectangle{ color: "#ccffff"; width: standardWidth; height: standardHeight;  transform: Rotation { origin.z: 0; origin.y: 0 ; axis{x: 0; y: 1; z:0} angle: 70; } smooth: smoothing}
        Rectangle{ color: "#99ffff"; width: standardWidth; height: standardHeight;  transform: Rotation { origin.z: 0; origin.y: 0 ; axis{x: 0; y: 1; z:0} angle: 80; } smooth: smoothing}
    }
}
