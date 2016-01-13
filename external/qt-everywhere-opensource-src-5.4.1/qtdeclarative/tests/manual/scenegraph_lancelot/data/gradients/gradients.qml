import QtQuick 2.0

Rectangle {
    id: r_0000
    width: 320
    height: 480
    color: "white"

    property int standardRectWidth:  48
    property int standardRectHeight:  48

    Text{
        z: 1
        text: "simple gradients"
        font.family: "Arial"
        font.pointSize: 20
        color: "black"
        font.bold: true
        anchors.verticalCenter: parent.Center
        anchors.horizontalCenter : parent.horizontalCenter
    }

    Rectangle{
        id: r_0001
        x: 0
        y: 0
        width: standardRectWidth
        height: standardRectHeight
        color: "red"
        gradient: Gradient{
            GradientStop{ position: 1.0; color: r_0001.color }
            GradientStop{ position: 0.0; color: "white" }
        }
    }
    Rectangle{
        id: r_0002
        x: r_0001.x + standardRectWidth
        y: 0
        width: standardRectWidth
        height: standardRectHeight
        color: "orange"
        gradient: Gradient{
            GradientStop{ position: 1.0; color: r_0002.color }
            GradientStop{ position: 0.0; color: "white" }
        }
    }
    Rectangle{
        id: r_0003
        x: r_0002.x + standardRectWidth
        y: 0
        width: standardRectWidth
        height: standardRectHeight
        color: "yellow"
        gradient: Gradient{
            GradientStop{ position: 1.0; color: r_0003.color }
            GradientStop{ position: 0.0; color: "white" }
        }
    }
    Rectangle{
        id: r_0004
        x: r_0003.x + standardRectWidth
        y: 0
        width: standardRectWidth
        height: standardRectHeight
        color: "green"
        gradient: Gradient{
            GradientStop{ position: 1.0; color: r_0004.color }
            GradientStop{ position: 0.0; color: "white" }
        }
    }
    Rectangle{
        id: r_0005
        x: r_0004.x + standardRectWidth
        y: 0
        width: standardRectWidth
        height: standardRectHeight
        color: "blue"
        gradient: Gradient{
            GradientStop{ position: 1.0; color: r_0005.color }
            GradientStop{ position: 0.0; color: "white" }
        }
    }
    Rectangle{
        id: r_0006
        x: r_0005.x + standardRectWidth
        y: 0
        width: standardRectWidth
        height: standardRectHeight
        color: "indigo"
        gradient: Gradient{
            GradientStop{ position: 1.0; color: r_0006.color }
            GradientStop{ position: 0.0; color: "white" }
        }
    }
    Rectangle{
        id: r_0007
        x: r_0001.x
        y: standardRectHeight
        width: standardRectWidth
        height: standardRectHeight
        color: "violet"
        gradient: Gradient{
            GradientStop{ position: 1.0; color: r_0007.color }
            GradientStop{ position: 0.0; color: "white" }
        }
    }
    Rectangle{
        id: r_0008
        x: r_0002.x
        y: standardRectHeight
        width: standardRectWidth
        height: standardRectHeight
        color: "black"
        gradient: Gradient{
            GradientStop{ position: 1.0; color: r_0008.color }
            GradientStop{ position: 0.0; color: "white" }
        }
    }
    Rectangle{
        id: r_0009
        x: r_0003.x
        y: standardRectHeight
        width: standardRectWidth
        height: standardRectHeight
        color: "dark grey"
        gradient: Gradient{
            GradientStop{ position: 1.0; color: r_0009.color }
            GradientStop{ position: 0.0; color: "white" }
        }
    }
    Rectangle{
        id: r_0010
        x: r_0004.x
        y: standardRectHeight
        width: standardRectWidth
        height: standardRectHeight
        color: "light grey"
        gradient: Gradient{
            GradientStop{ position: 1.0; color: r_0010.color }
            GradientStop{ position: 0.0; color: "white" }
        }
    }
    Rectangle{
        id: r_0011
        x: r_0005.x
        y: standardRectHeight
        width: standardRectWidth
        height: standardRectHeight
        color: "pink"
        gradient: Gradient{
            GradientStop{ position: 1.0; color: r_0011.color }
            GradientStop{ position: 0.0; color: "white" }
        }
    }
    Rectangle{
        id: r_0012
        x: r_0006.x
        y: standardRectHeight
        width: standardRectWidth
        height: standardRectHeight
        color: "gold"
        gradient: Gradient{
            GradientStop{ position: 1.0; color: r_0012.color }
            GradientStop{ position: 0.0; color: "white" }
        }
    }

    Rectangle{
        id: r_0013
        x: 0
        y: (standardRectHeight * 2)
        color: "green"
        width: 3 * standardRectWidth
        height: 3 * standardRectHeight
        gradient: Gradient{
            GradientStop{ position: 0.0; color: "yellow"}
            GradientStop{ position: 0.25; color: "orange"}
            GradientStop{ position: 0.75; color: "purple"}
            GradientStop{ position: 1.0; color: "blue"}
        }
    }
    Rectangle{
        id: r_0014
        x: 3 * standardRectWidth
        y: (standardRectHeight * 2)
        color: "red"
        width: 3 * standardRectWidth
        height: 3 * standardRectHeight
        gradient: Gradient{
            GradientStop{ position: 0.0; color: "blue"}
            GradientStop{ position: 0.25; color: "purple"}
            GradientStop{ position: 0.75; color: "red"}
            GradientStop{ position: 1.0; color: "violet"}
        }
    }
    Rectangle{
        id: r_00015
        x: 0
        y: 5 * standardRectHeight
        width: 6 * standardRectWidth
        height: 6 * standardRectHeight
        color: "black"
        gradient: Gradient{
        GradientStop{ position: 0.0; color: "cyan"}
        GradientStop{ position: 0.1; color: "purple"}
        GradientStop{ position: 0.2; color: "red"}
        GradientStop{ position: 0.3; color: "yellow"}
        GradientStop{ position: 0.4; color: "blue"}
        GradientStop{ position: 0.5; color: "white"}
        GradientStop{ position: 0.55; color: "red"}
        GradientStop{ position: 0.6; color: "violet"}
        GradientStop{ position: 0.7; color: "blue"}
        GradientStop{ position: 0.8; color: "green"}
        GradientStop{ position: 0.9; color: "red"}
        GradientStop{ position: 1.0; color: "violet"}
        }
    }
}

