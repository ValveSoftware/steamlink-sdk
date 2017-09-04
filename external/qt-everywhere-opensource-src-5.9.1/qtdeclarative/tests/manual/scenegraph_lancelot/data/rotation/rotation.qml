import QtQuick 2.0

Rectangle {
    width: 320
    height: 480
    smooth: smoothing
    property bool smoothing: true
    property int standardWidth: 30
    property int standardHeight: 30
    property int standardSpacing: 10
    Grid{
        id: grid_0000
        anchors.top: parent.baseline
        anchors.left: parent.left
        columns: 8
        spacing: standardSpacing
        Rectangle{ color: "red"; width: standardWidth; height: standardHeight; rotation: 0; smooth: smoothing}
        Rectangle{ color: "orange"; width: standardWidth; height: standardHeight; rotation: 10; smooth: smoothing}
        Rectangle{ color: "yellow"; width: standardWidth; height: standardHeight; rotation: 20; smooth: smoothing}
        Rectangle{ color: "blue"; width: standardWidth; height: standardHeight; rotation: 30; smooth: smoothing}
        Rectangle{ color: "green"; width: standardWidth; height: standardHeight; rotation: 40; smooth: smoothing}
        Rectangle{ color: "indigo"; width: standardWidth; height: standardHeight; rotation: 50; smooth: smoothing}
        Rectangle{ color: "violet"; width: standardWidth; height: standardHeight; rotation: 60; smooth: smoothing}
        Rectangle{ color: "light green"; width: standardWidth; height: standardHeight; rotation: 70; smooth: smoothing}
        Rectangle{ color: "light blue"; width: standardWidth; height: standardHeight; rotation: 80; smooth: smoothing}
        Rectangle{ color: "light grey"; width: standardWidth; height: standardHeight; rotation: 5; smooth: smoothing}
        Rectangle{ color: "black"; width: standardWidth; height: standardHeight; rotation: 15; smooth: smoothing}
        Rectangle{ color: "dark grey"; width: standardWidth; height: standardHeight; rotation: 25; smooth: smoothing}
        Rectangle{ color: "purple"; width: standardWidth; height: standardHeight; rotation: 35; smooth: smoothing}
        Rectangle{ color: "pink"; width: standardWidth; height: standardHeight; rotation: 45; smooth: smoothing}
        Rectangle{ color: "cyan"; width: standardWidth; height: standardHeight; rotation: 55; smooth: smoothing}
        Rectangle{ color: "brown"; width: standardWidth; height: standardHeight; rotation: 65; smooth: smoothing}
    }
    Grid{
        id: grid_0001
        anchors.top: grid_0000.bottom
        anchors.left: grid_0000.left
        columns: 8
        spacing: standardSpacing
        Rectangle{ color: "#ff0000"; width: standardWidth; height: standardHeight; rotation: 0; smooth: smoothing}
        Rectangle{ color: "#ff3333"; width: standardWidth; height: standardHeight; rotation: 10; smooth: smoothing}
        Rectangle{ color: "#ff6666"; width: standardWidth; height: standardHeight; rotation: 20; smooth: smoothing}
        Rectangle{ color: "#ff9999"; width: standardWidth; height: standardHeight; rotation: 30; smooth: smoothing}
        Rectangle{ color: "#ffcccc"; width: standardWidth; height: standardHeight; rotation: 40; smooth: smoothing}
        Rectangle{ color: "#ffeeff"; width: standardWidth; height: standardHeight; rotation: 50; smooth: smoothing}
        Rectangle{ color: "#ccffff"; width: standardWidth; height: standardHeight; rotation: 60; smooth: smoothing}
        Rectangle{ color: "#99ffff"; width: standardWidth; height: standardHeight; rotation: 70; smooth: smoothing}
        Rectangle{ color: "#66ffff"; width: standardWidth; height: standardHeight; rotation: 80; smooth: smoothing}
        Rectangle{ color: "#33ffff"; width: standardWidth; height: standardHeight; rotation: 5; smooth: smoothing}
        Rectangle{ color: "#00ffff"; width: standardWidth; height: standardHeight; rotation: 15; smooth: smoothing}
        Rectangle{ color: "#ff00cc"; width: standardWidth; height: standardHeight; rotation: 25; smooth: smoothing}
        Rectangle{ color: "#ff33ff"; width: standardWidth; height: standardHeight; rotation: 35; smooth: smoothing}
        Rectangle{ color: "#ff66ff"; width: standardWidth; height: standardHeight; rotation: 45; smooth: smoothing}
        Rectangle{ color: "#cc66ff"; width: standardWidth; height: standardHeight; rotation: 55; smooth: smoothing}
        Rectangle{ color: "#9966ff"; width: standardWidth; height: standardHeight; rotation: 65; smooth: smoothing}
    }
    Grid{
        id: grid_0002
        anchors.top: grid_0001.bottom
        anchors.left: grid_0001.left
        columns: 8
        spacing: standardSpacing
        Rectangle{ color: "#ff0010"; width: standardWidth; height: standardHeight; rotation: 80; smooth: smoothing}
        Rectangle{ color: "#ff3323"; width: standardWidth; height: standardHeight; rotation: 70; smooth: smoothing}
        Rectangle{ color: "#ff6636"; width: standardWidth; height: standardHeight; rotation: 60; smooth: smoothing}
        Rectangle{ color: "#ff9949"; width: standardWidth; height: standardHeight; rotation: 50; smooth: smoothing}
        Rectangle{ color: "#ffcc5c"; width: standardWidth; height: standardHeight; rotation: 40; smooth: smoothing}
        Rectangle{ color: "#ffee6f"; width: standardWidth; height: standardHeight; rotation: 30; smooth: smoothing}
        Rectangle{ color: "#ccff7f"; width: standardWidth; height: standardHeight; rotation: 20; smooth: smoothing}
        Rectangle{ color: "#99ff8f"; width: standardWidth; height: standardHeight; rotation: 10; smooth: smoothing}
        Rectangle{ color: "#66ff9f"; width: standardWidth; height: standardHeight; rotation: -10; smooth: smoothing}
        Rectangle{ color: "#33ffaf"; width: standardWidth; height: standardHeight; rotation: -20; smooth: smoothing}
        Rectangle{ color: "#00ffbf"; width: standardWidth; height: standardHeight; rotation: -30; smooth: smoothing}
        Rectangle{ color: "#ff00cc"; width: standardWidth; height: standardHeight; rotation: -40; smooth: smoothing}
        Rectangle{ color: "#ff33df"; width: standardWidth; height: standardHeight; rotation: -50; smooth: smoothing}
        Rectangle{ color: "#ff66ef"; width: standardWidth; height: standardHeight; rotation: -60; smooth: smoothing}
        Rectangle{ color: "#cc661f"; width: standardWidth; height: standardHeight; rotation: -70; smooth: smoothing}
        Rectangle{ color: "#99662f"; width: standardWidth; height: standardHeight; rotation: -80; smooth: smoothing}
    }
    Grid{
        id: grid_0003
        anchors.top: grid_0002.bottom
        anchors.left: grid_0002.left
        columns: 8
        spacing: standardSpacing
        Rectangle{ color: "#ffcc00"; width: standardWidth; height: standardHeight; rotation: 80; smooth: smoothing}
        Rectangle{ color: "#ffff33"; width: standardWidth; height: standardHeight; rotation: 70; smooth: smoothing}
        Rectangle{ color: "#ccff33"; width: standardWidth; height: standardHeight; rotation: 60; smooth: smoothing}
        Rectangle{ color: "#99ff33"; width: standardWidth; height: standardHeight; rotation: 50; smooth: smoothing}
        Rectangle{ color: "#66ff33"; width: standardWidth; height: standardHeight; rotation: 40; smooth: smoothing}
        Rectangle{ color: "#33ff33"; width: standardWidth; height: standardHeight; rotation: 30; smooth: smoothing}
        Rectangle{ color: "#00ff33"; width: standardWidth; height: standardHeight; rotation: 20; smooth: smoothing}
        Rectangle{ color: "#ff00ff"; width: standardWidth; height: standardHeight; rotation: 10; smooth: smoothing}
        Rectangle{ color: "#cc00ff"; width: standardWidth; height: standardHeight; rotation: -10; smooth: smoothing}
        Rectangle{ color: "#9900ff"; width: standardWidth; height: standardHeight; rotation: -15; smooth: smoothing}
        Rectangle{ color: "#6600ff"; width: standardWidth; height: standardHeight; rotation: -20; smooth: smoothing}
        Rectangle{ color: "#3300ff"; width: standardWidth; height: standardHeight; rotation: -25; smooth: smoothing}
        Rectangle{ color: "#0000ff"; width: standardWidth; height: standardHeight; rotation: -30; smooth: smoothing}
        Rectangle{ color: "#ff00cc"; width: standardWidth; height: standardHeight; rotation: -35; smooth: smoothing}
        Rectangle{ color: "#ff33cc"; width: standardWidth; height: standardHeight; rotation: -40; smooth: smoothing}
        Rectangle{ color: "#cc66ff"; width: standardWidth; height: standardHeight; rotation: -45; smooth: smoothing}
    }
    Grid{
        id: grid_0004
        anchors.top: grid_0003.bottom
        anchors.left: grid_0003.left
        columns: 8
        spacing: standardSpacing
        Rectangle{ color: "#eecc00"; width: standardWidth; height: standardHeight; rotation: 80; smooth: smoothing}
        Rectangle{ color: "#eeff33"; width: standardWidth; height: standardHeight; rotation: 70; smooth: smoothing}
        Rectangle{ color: "#ccff33"; width: standardWidth; height: standardHeight; rotation: 60; smooth: smoothing}
        Rectangle{ color: "#99ff33"; width: standardWidth; height: standardHeight; rotation: 50; smooth: smoothing}
        Rectangle{ color: "#66ff33"; width: standardWidth; height: standardHeight; rotation: 40; smooth: smoothing}
        Rectangle{ color: "#44ff33"; width: standardWidth; height: standardHeight; rotation: 30; smooth: smoothing}
        Rectangle{ color: "#00ff33"; width: standardWidth; height: standardHeight; rotation: 20; smooth: smoothing}
        Rectangle{ color: "#ee00ff"; width: standardWidth; height: standardHeight; rotation: 10; smooth: smoothing}
        Rectangle{ color: "#cc00ff"; width: standardWidth; height: standardHeight; rotation: -10; smooth: smoothing}
        Rectangle{ color: "#9900ff"; width: standardWidth; height: standardHeight; rotation: -15; smooth: smoothing}
        Rectangle{ color: "#6600ff"; width: standardWidth; height: standardHeight; rotation: -20; smooth: smoothing}
        Rectangle{ color: "#4400ff"; width: standardWidth; height: standardHeight; rotation: -25; smooth: smoothing}
        Rectangle{ color: "#0000ff"; width: standardWidth; height: standardHeight; rotation: -30; smooth: smoothing}
        Rectangle{ color: "#ee00cc"; width: standardWidth; height: standardHeight; rotation: -35; smooth: smoothing}
        Rectangle{ color: "#ee33cc"; width: standardWidth; height: standardHeight; rotation: -40; smooth: smoothing}
        Rectangle{ color: "#cc66ff"; width: standardWidth; height: standardHeight; rotation: -45; smooth: smoothing}
    }
    Grid{
        id: grid_0005
        anchors.top: grid_0004.bottom
        anchors.left: grid_0004.left
        columns: 8
        spacing: standardSpacing
        Rectangle{ color: "red"; width: standardWidth; height: standardHeight; rotation: 0; smooth: smoothing}
        Rectangle{ color: "orange"; width: standardWidth; height: standardHeight; rotation: 10; smooth: smoothing}
        Rectangle{ color: "yellow"; width: standardWidth; height: standardHeight; rotation: 20; smooth: smoothing}
        Rectangle{ color: "blue"; width: standardWidth; height: standardHeight; rotation: 30; smooth: smoothing}
        Rectangle{ color: "green"; width: standardWidth; height: standardHeight; rotation: 40; smooth: smoothing}
        Rectangle{ color: "indigo"; width: standardWidth; height: standardHeight; rotation: 50; smooth: smoothing}
        Rectangle{ color: "violet"; width: standardWidth; height: standardHeight; rotation: 60; smooth: smoothing}
        Rectangle{ color: "light green"; width: standardWidth; height: standardHeight; rotation: 70; smooth: smoothing}
        Rectangle{ color: "light blue"; width: standardWidth; height: standardHeight; rotation: 80; smooth: smoothing}
        Rectangle{ color: "light grey"; width: standardWidth; height: standardHeight; rotation: 5; smooth: smoothing}
        Rectangle{ color: "black"; width: standardWidth; height: standardHeight; rotation: 15; smooth: smoothing}
        Rectangle{ color: "dark grey"; width: standardWidth; height: standardHeight; rotation: 25; smooth: smoothing}
        Rectangle{ color: "purple"; width: standardWidth; height: standardHeight; rotation: 35; smooth: smoothing}
        Rectangle{ color: "pink"; width: standardWidth; height: standardHeight; rotation: 45; smooth: smoothing}
        Rectangle{ color: "cyan"; width: standardWidth; height: standardHeight; rotation: 55; smooth: smoothing}
        Rectangle{ color: "brown"; width: standardWidth; height: standardHeight; rotation: 65; smooth: smoothing}
    }
    Grid{
        id: grid_0006
        anchors.top: grid_0005.bottom
        anchors.left: grid_0005.left
        columns: 8
        spacing: standardSpacing
        Rectangle{ color: "#ff6600"; width: standardWidth; height: standardHeight; rotation: 0; smooth: smoothing}
        Rectangle{ color: "#cc6600"; width: standardWidth; height: standardHeight; rotation: 10; smooth: smoothing}
        Rectangle{ color: "#996600"; width: standardWidth; height: standardHeight; rotation: 20; smooth: smoothing}
        Rectangle{ color: "#666600"; width: standardWidth; height: standardHeight; rotation: 30; smooth: smoothing}
        Rectangle{ color: "#336600"; width: standardWidth; height: standardHeight; rotation: 40; smooth: smoothing}
        Rectangle{ color: "#006600"; width: standardWidth; height: standardHeight; rotation: 50; smooth: smoothing}
        Rectangle{ color: "#009933"; width: standardWidth; height: standardHeight; rotation: 60; smooth: smoothing}
        Rectangle{ color: "#00cc66"; width: standardWidth; height: standardHeight; rotation: 70; smooth: smoothing}
        Rectangle{ color: "#ff0066"; width: standardWidth; height: standardHeight; rotation: 80; smooth: smoothing}
        Rectangle{ color: "#cc0066"; width: standardWidth; height: standardHeight; rotation: 5; smooth: smoothing}
        Rectangle{ color: "#990066"; width: standardWidth; height: standardHeight; rotation: 15; smooth: smoothing}
        Rectangle{ color: "#660066"; width: standardWidth; height: standardHeight; rotation: 25; smooth: smoothing}
        Rectangle{ color: "#330066"; width: standardWidth; height: standardHeight; rotation: 35; smooth: smoothing}
        Rectangle{ color: "#000066"; width: standardWidth; height: standardHeight; rotation: 45; smooth: smoothing}
        Rectangle{ color: "#003399"; width: standardWidth; height: standardHeight; rotation: 55; smooth: smoothing}
        Rectangle{ color: "#0066cc"; width: standardWidth; height: standardHeight; rotation: 65; smooth: smoothing}
    }
}
