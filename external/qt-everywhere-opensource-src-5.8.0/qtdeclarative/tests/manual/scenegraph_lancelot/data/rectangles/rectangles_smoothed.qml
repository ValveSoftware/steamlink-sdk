import QtQuick 2.0

Rectangle {
    id: r_0000
    width: 320
    height: 480
    color: "white"

    property int standardRectWidth:  22
    property int standardRectHeight:  40
    property int xOffsetPlacement: 10
    property int yOffsetPlacement: 18
    property int borderSize: 2
    property int yPlacementRow_0: 0
    property int yPlacementRow_1: (standardRectHeight+yOffsetPlacement)
    property int yPlacementRow_2: yPlacementRow_1 + (standardRectHeight+yOffsetPlacement)
    property int yPlacementRow_3: yPlacementRow_2 + (standardRectHeight+yOffsetPlacement)
    property int yPlacementRow_4: yPlacementRow_3 + (standardRectHeight+yOffsetPlacement)
    property int yPlacementRow_5: yPlacementRow_4 + (standardRectHeight+yOffsetPlacement)
    property int yPlacementRow_6: yPlacementRow_5 + (standardRectHeight+yOffsetPlacement)
    property int yPlacementRow_7: yPlacementRow_6 + (standardRectHeight+yOffsetPlacement)
    property int yPlacementRow_8: yPlacementRow_7 + (standardRectHeight+yOffsetPlacement)
    property int yPlacementRow_9: yPlacementRow_8 + (standardRectHeight+yOffsetPlacement)
    property int xPlacementCol_0: 0
    property int xPlacementCol_1: xPlacementCol_0 + (standardRectWidth+xOffsetPlacement)
    property int xPlacementCol_2: xPlacementCol_1 + (standardRectWidth+xOffsetPlacement)
    property int xPlacementCol_3: xPlacementCol_2 + (standardRectWidth+xOffsetPlacement)
    property int xPlacementCol_4: xPlacementCol_3 + (standardRectWidth+xOffsetPlacement)
    property int xPlacementCol_5: xPlacementCol_4 + (standardRectWidth+xOffsetPlacement)
    property int xPlacementCol_6: xPlacementCol_5 + (standardRectWidth+xOffsetPlacement)
    property int xPlacementCol_7: xPlacementCol_6 + (standardRectWidth+xOffsetPlacement)
    property int xPlacementCol_8: xPlacementCol_7 + (standardRectWidth+xOffsetPlacement)
    property int xPlacementCol_9: xPlacementCol_8 + (standardRectWidth+xOffsetPlacement)
    property bool smoothingOn: true
    property real scaleFactor: 1.2


    //Test basic color
    Rectangle{
        id: r_0001
        smooth: smoothingOn
        scale: scaleFactor
        x: xPlacementCol_0
        y: yPlacementRow_0
        width: r_0000.standardRectWidth
        height: r_0000.standardRectHeight
        color: "red"
    }
    Rectangle{
        id: r_0002
        smooth: smoothingOn
        scale: scaleFactor
        x: xPlacementCol_1
        y: yPlacementRow_0
        width: standardRectWidth
        height: standardRectHeight
        color: "orange"
    }
    Rectangle{
        id: r_0003
        smooth: smoothingOn
        scale: scaleFactor
        x: xPlacementCol_2
        y: yPlacementRow_0
        width: standardRectWidth
        height: standardRectHeight
        color: "yellow"
    }
    Rectangle{
        id: r_0004
        smooth: smoothingOn
        scale: scaleFactor
        x: xPlacementCol_3
        y: yPlacementRow_0
        width: standardRectWidth
        height: standardRectHeight
        color: "green"
    }
    Rectangle {
        id: r_0005
        smooth: smoothingOn
        scale: scaleFactor
        x: xPlacementCol_4
        y: yPlacementRow_0
        width: standardRectWidth
        height: standardRectHeight
        color: "blue"
    }
    Rectangle{
        id: r_0006
        smooth: smoothingOn
        scale: scaleFactor
        x: xPlacementCol_5
        y: yPlacementRow_0
        width: standardRectWidth
        height: standardRectHeight
        color: "indigo"
    }
    Rectangle{
        id: r_0007
        smooth: smoothingOn
        scale: scaleFactor
        x: xPlacementCol_6
        y: yPlacementRow_0
        width: standardRectWidth
        height: standardRectHeight
        color: "violet"
    }
    Rectangle{
        id: r_0008
        smooth: smoothingOn
        scale: scaleFactor
        x: xPlacementCol_7
        y: yPlacementRow_0
        width: standardRectWidth
        height: standardRectHeight
        color: "black"
    }
    Rectangle{
        id: r_0009
        smooth: smoothingOn
        scale: scaleFactor
        x: xPlacementCol_8
        y: yPlacementRow_0
        width: standardRectWidth
        height: standardRectHeight
        color: "dark grey"
    }
    Rectangle{
        id: r_0010
        smooth: smoothingOn
        scale: scaleFactor
        x: xPlacementCol_9
        y: yPlacementRow_0
        width: standardRectWidth
        height: standardRectHeight
        color: "light grey"
    }
    Text{
        id: annotation_row0
        z: 1
        text: "SMOOTHED SCALED RECTANGLES"
        font.family: "Arial"
        font.pointSize: 15
        color: "black"
        font.bold: true
        anchors.verticalCenter : r_0001.verticalCenter
    }

    //Test borders
    Rectangle{
        id: r_0011
        x: xPlacementCol_0
        y: yPlacementRow_1
        smooth: smoothingOn
        scale: scaleFactor
        width: standardRectWidth
        height: standardRectHeight
        color: "red"
        border.width: borderSize
        border.color: "orange"

    }
    Rectangle{
        id: r_0012
        x: xPlacementCol_1
        y: yPlacementRow_1
        smooth: smoothingOn
        scale: scaleFactor
        width: standardRectWidth
        height: standardRectHeight
        color: "orange"
        border.width: borderSize
        border.color: "yellow"

    }
    Rectangle{
        id: r_0013
        x: xPlacementCol_2
        y: yPlacementRow_1
        smooth: smoothingOn
        scale: scaleFactor
        width: standardRectWidth
        height: standardRectHeight
        color: "yellow"
        border.width: borderSize
        border.color: "green"

    }
    Rectangle{
        id: r_0014
        x: xPlacementCol_3
        y: yPlacementRow_1
        smooth: smoothingOn
        scale: scaleFactor
        width: standardRectWidth
        height: standardRectHeight
        color: "green"
        border.width: borderSize
        border.color: "blue"

    }
    Rectangle{
        id: r_0015
        x: xPlacementCol_4
        y: yPlacementRow_1
        smooth: smoothingOn
        scale: scaleFactor
        width: standardRectWidth
        height: standardRectHeight
        color: "blue"
        border.width: borderSize
        border.color: "indigo"

    }
    Rectangle{
        id: r_0016
        x: xPlacementCol_5
        y: yPlacementRow_1
        smooth: smoothingOn
        scale: scaleFactor
        width: standardRectWidth
        height: standardRectHeight
        color: "indigo"
        border.width: borderSize
        border.color: "violet"

    }
    Rectangle{
        id: r_0017
        x: xPlacementCol_6
        y: yPlacementRow_1
        smooth: smoothingOn
        scale: scaleFactor
        width: standardRectWidth
        height: standardRectHeight
        color: "violet"
        border.width: borderSize
        border.color: "black"

    }
    Rectangle{
        id: r_0018
        x: xPlacementCol_7
        y: yPlacementRow_1
        smooth: smoothingOn
        scale: scaleFactor
        width: standardRectWidth
        height: standardRectHeight
        color: "black"
        border.width: borderSize
        border.color: "dark grey"

    }
    Rectangle{
        id: r_0019
        x: xPlacementCol_8
        y: yPlacementRow_1
        smooth: smoothingOn
        scale: scaleFactor
        width: standardRectWidth
        height: standardRectHeight
        color: "dark grey"
        border.width: borderSize
        border.color: "light grey"

    }
    Rectangle{
        id: r_0020
        x: xPlacementCol_9
        y: yPlacementRow_1
        smooth: smoothingOn
        scale: scaleFactor
        width: standardRectWidth
        height: standardRectHeight
        color: "light grey"
        border.width: borderSize
        border.color: "red"

    }
    Text{
        id: annotation_row1
        z: 1
        text: "SMOOTHED SCALED BORDERS"
        font.family: "Arial"
        font.pointSize: 15
        color: "black"
        font.bold: true
        anchors.verticalCenter : r_0011.verticalCenter
    }

    //Test Gradients
    Rectangle{
        id: r_0021
        x: xPlacementCol_0
        y: yPlacementRow_2
        smooth: smoothingOn
        scale: scaleFactor
        width: standardRectWidth
        height: standardRectHeight
        color: "red"
        gradient: Gradient{
            GradientStop{ position: 1.0; color: r_0021.color }
            GradientStop{ position: 0.0; color: "white" }
        }

    }
    Rectangle{
        id: r_0022
        x: xPlacementCol_1
        y: yPlacementRow_2
        smooth: smoothingOn
        scale: scaleFactor
        width: standardRectWidth
        height: standardRectHeight
        color: "orange"
        gradient: Gradient{
            GradientStop{ position: 1.0; color: r_0022.color }
            GradientStop{ position: 0.0; color: "white" }
        }

    }
    Rectangle{
        id: r_0023
        x: xPlacementCol_2
        y: yPlacementRow_2
        smooth: smoothingOn
        scale: scaleFactor
        width: standardRectWidth
        height: standardRectHeight
        color: "yellow"
        gradient: Gradient{
            GradientStop{ position: 1.0; color: r_0023.color }
            GradientStop{ position: 0.0; color: "white" }
        }

    }
    Rectangle{
        id: r_0024
        x: xPlacementCol_3
        y: yPlacementRow_2
        smooth: smoothingOn
        scale: scaleFactor
        width: standardRectWidth
        height: standardRectHeight
        color: "green"
        gradient: Gradient{
            GradientStop{ position: 1.0; color: r_0024.color }
            GradientStop{ position: 0.0; color: "white" }
        }

    }
    Rectangle{
        id: r_0025
        x: xPlacementCol_4
        y: yPlacementRow_2
        smooth: smoothingOn
        scale: scaleFactor
        width: standardRectWidth
        height: standardRectHeight
        color: "blue"
        gradient: Gradient{
            GradientStop{ position: 1.0; color: r_0025.color }
            GradientStop{ position: 0.0; color: "white" }
        }

    }
    Rectangle{
        id: r_0026
        x: xPlacementCol_5
        y: yPlacementRow_2
        smooth: smoothingOn
        scale: scaleFactor
        width: standardRectWidth
        height: standardRectHeight
        color: "indigo"
        gradient: Gradient{
            GradientStop{ position: 1.0; color: r_0026.color }
            GradientStop{ position: 0.0; color: "white" }
        }

    }
    Rectangle{
        id: r_0027
        x: xPlacementCol_6
        y: yPlacementRow_2
        smooth: smoothingOn
        scale: scaleFactor
        width: standardRectWidth
        height: standardRectHeight
        color: "violet"
        gradient: Gradient{
            GradientStop{ position: 1.0; color: r_0027.color }
            GradientStop{ position: 0.0; color: "white" }
        }

    }
    Rectangle{
        id: r_0028
        x: xPlacementCol_7
        y: yPlacementRow_2
        smooth: smoothingOn
        scale: scaleFactor
        width: standardRectWidth
        height: standardRectHeight
        color: "black"
        gradient: Gradient{
            GradientStop{ position: 1.0; color: r_0028.color }
            GradientStop{ position: 0.0; color: "white" }
        }

    }
    Rectangle{
        id: r_0029
        x: xPlacementCol_8
        y: yPlacementRow_2
        smooth: smoothingOn
        scale: scaleFactor
        width: standardRectWidth
        height: standardRectHeight
        color: "dark grey"
        gradient: Gradient{
            GradientStop{ position: 1.0; color: r_0029.color }
            GradientStop{ position: 0.0; color: "white" }
        }

    }
    Rectangle{
        id: r_0030
        x: xPlacementCol_9
        y: yPlacementRow_2
        smooth: smoothingOn
        scale: scaleFactor
        width: standardRectWidth
        height: standardRectHeight
        color: "light grey"
        gradient: Gradient{
            GradientStop{ position: 1.0; color: r_0030.color }
            GradientStop{ position: 0.0; color: "white" }
        }

    }
    Text{
        id: annotation_row2
        z: 1
        text: "SMOOTHED SCALED GRADIENTS"
        font.family: "Arial"
        font.pointSize: 15
        color: "black"
        font.bold: true
        anchors.verticalCenter : r_0021.verticalCenter
    }

    //Test radius
    Rectangle{
        id: r_0031
        x: xPlacementCol_0
        y: yPlacementRow_3
        smooth: smoothingOn
        scale: scaleFactor
        width: r_0000.standardRectWidth
        height: r_0000.standardRectHeight
        color: "red"
        radius: 1

    }
    Rectangle{
        id: r_0032
        x: xPlacementCol_1
        y: yPlacementRow_3
        smooth: smoothingOn
        scale: scaleFactor
        width: standardRectWidth
        height: standardRectHeight
        color: "orange"
        radius: 2

    }
    Rectangle{
        id: r_0033
        x: xPlacementCol_2
        y: yPlacementRow_3
        smooth: smoothingOn
        scale: scaleFactor
        width: standardRectWidth
        height: standardRectHeight
        color: "yellow"
        radius: 3

    }
    Rectangle{
        id: r_0034
        x: xPlacementCol_3
        y: yPlacementRow_3
        smooth: smoothingOn
        scale: scaleFactor
        width: standardRectWidth
        height: standardRectHeight
        color: "green"
        radius: 4

    }
    Rectangle{
        id: r_0035
        x: xPlacementCol_4
        y: yPlacementRow_3
        smooth: smoothingOn
        scale: scaleFactor
        width: standardRectWidth
        height: standardRectHeight
        color: "blue"
        radius: 5

    }
    Rectangle{
        id: r_0036
        x: xPlacementCol_5
        y: yPlacementRow_3
        smooth: smoothingOn
        scale: scaleFactor
        width: standardRectWidth
        height: standardRectHeight
        color: "indigo"
        radius: 6

    }
    Rectangle{
        x: xPlacementCol_6
        y: yPlacementRow_3
        id: r_0037
        smooth: smoothingOn
        scale: scaleFactor
        width: standardRectWidth
        height: standardRectHeight
        color: "violet"
        radius: 7

    }
    Rectangle{
        x: xPlacementCol_7
        y: yPlacementRow_3
        id: r_0038
        smooth: smoothingOn
        scale: scaleFactor
        width: standardRectWidth
        height: standardRectHeight
        color: "black"
        radius: 8

    }
    Rectangle{
        x: xPlacementCol_8
        y: yPlacementRow_3
        id: r_0039
        smooth: smoothingOn
        scale: scaleFactor
        width: standardRectWidth
        height: standardRectHeight
        color: "dark grey"
        radius: 9

    }
    Rectangle{
        x: xPlacementCol_9
        y: yPlacementRow_3
        id: r_0040
        smooth: smoothingOn
        scale: scaleFactor
        width: standardRectWidth
        height: standardRectHeight
        color: "light grey"
        radius: 10

    }
    Text{
        id: annotation_row3
        z: 1
        text: "SMOOTHED SCALED RADIUS"
        font.family: "Arial"
        font.pointSize: 15
        color: "black"
        font.bold: true
        anchors.verticalCenter : r_0031.verticalCenter
    }
}

