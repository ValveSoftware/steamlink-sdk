import QtQuick 2.0

Rectangle {
    id: r_0000
    width: 320
    height: 480
    color: "white"

    property int standardRectWidth:  32
    property int standardRectHeight:  48
    property int textAnnotationXOffset: 32
    property int textAnnotationYOffset: 10
    property int borderSize: 2
    property int yPlacementRow_0: 0
    property int yPlacementRow_1: 48
    property int yPlacementRow_2: 96
    property int yPlacementRow_3: 144
    property int yPlacementRow_4: 192
    property int yPlacementRow_5: 240
    property int yPlacementRow_6: 288
    property int yPlacementRow_7: 336
    property int yPlacementRow_8: 384
    property int yPlacementRow_9: 432
    property int xPlacementCol_0: 0
    property int xPlacementCol_1: standardRectWidth
    property int xPlacementCol_2: standardRectWidth*2
    property int xPlacementCol_3: standardRectWidth*3
    property int xPlacementCol_4: standardRectWidth*4
    property int xPlacementCol_5: standardRectWidth*5
    property int xPlacementCol_6: standardRectWidth*6
    property int xPlacementCol_7: standardRectWidth*7
    property int xPlacementCol_8: standardRectWidth*8
    property int xPlacementCol_9: standardRectWidth*9

    Component{
        id: annotation
        Text{
            width: 160
            height: 240
            x: textAnnotationXOffset
            y: textAnnotationYOffset
            z: 1
            text: annotationTextLabel
            font.family: "Arial"
            font.pointSize: 15
            color: "white"
            font.bold: true
        }
    }
    //Test basic color
    Rectangle{ smooth: false
        id: r_0001
        x: 0
        y: yPlacementRow_0
        width: r_0000.standardRectWidth
        height: r_0000.standardRectHeight
        color: "red"
    }
    Rectangle{ smooth: false
        id: r_0002
        x: r_0001.x + standardRectWidth
        y: yPlacementRow_0
        width: standardRectWidth
        height: standardRectHeight
        color: "orange"
    }
    Rectangle{ smooth: false
        id: r_0003
        x: r_0002.x + standardRectWidth
        y: yPlacementRow_0
        width: standardRectWidth
        height: standardRectHeight
        color: "yellow"
    }
    Rectangle{ smooth: false
        id: r_0004
        x: r_0003.x + standardRectWidth
        y: yPlacementRow_0
        width: standardRectWidth
        height: standardRectHeight
        color: "green"
    }
    Rectangle {
        id: r_0005
        x: r_0004.x + standardRectWidth
        y: yPlacementRow_0
        width: standardRectWidth
        height: standardRectHeight
        color: "blue"
    }
    Rectangle{ smooth: false
        id: r_0006
        x: r_0005.x + standardRectWidth
        y: yPlacementRow_0
        width: standardRectWidth
        height: standardRectHeight
        color: "indigo"
    }
    Rectangle{ smooth: false
        id: r_0007
        x: r_0006.x + standardRectWidth
        y: yPlacementRow_0
        width: standardRectWidth
        height: standardRectHeight
        color: "violet"
    }
    Rectangle{ smooth: false
        id: r_0008
        x: r_0007.x + standardRectWidth
        y: yPlacementRow_0
        width: standardRectWidth
        height: standardRectHeight
        color: "black"
    }
    Rectangle{ smooth: false
        id: r_0009
        x: r_0008.x + standardRectWidth
        y: yPlacementRow_0
        width: standardRectWidth
        height: standardRectHeight
        color: "dark grey"
    }
    Rectangle{ smooth: false
        id: r_0010
        x: r_0009.x + standardRectWidth
        y: yPlacementRow_0
        width: standardRectWidth
        height: standardRectHeight
        color: "light grey"
    }
    Loader{
        sourceComponent: annotation;
        property string annotationTextLabel: "BASIC COLOR RECTANGLES";
        property int textAnnotationXOffset: 4;
        property int textAnnotationYOffset: 10;
    }
    //Test borders
    Rectangle{ smooth: false
        id: r_0011
        width: standardRectWidth
        height: standardRectHeight
        color: "red"
        border.width: borderSize
        border.color: "orange"
        anchors.horizontalCenter: r_0001.horizontalCenter
        anchors.top: r_0001.bottom
    }
    Rectangle{ smooth: false
        id: r_0012
        width: standardRectWidth
        height: standardRectHeight
        color: "orange"
        border.width: borderSize
        border.color: "yellow"
        anchors.horizontalCenter: r_0002.horizontalCenter
        anchors.top: r_0002.bottom
    }
    Rectangle{ smooth: false
        id: r_0013
        width: standardRectWidth
        height: standardRectHeight
        color: "yellow"
        border.width: borderSize
        border.color: "green"
        anchors.horizontalCenter: r_0003.horizontalCenter
        anchors.top: r_0003.bottom
    }
    Rectangle{ smooth: false
        id: r_0014
        width: standardRectWidth
        height: standardRectHeight
        color: "green"
        border.width: borderSize
        border.color: "blue"
        anchors.horizontalCenter: r_0004.horizontalCenter
        anchors.top: r_0004.bottom
    }
    Rectangle{ smooth: false
        id: r_0015
        width: standardRectWidth
        height: standardRectHeight
        color: "blue"
        border.width: borderSize
        border.color: "indigo"
        anchors.horizontalCenter: r_0005.horizontalCenter
        anchors.top: r_0005.bottom
    }
    Rectangle{ smooth: false
        id: r_0016
        width: standardRectWidth
        height: standardRectHeight
        color: "indigo"
        border.width: borderSize
        border.color: "violet"
        anchors.horizontalCenter: r_0006.horizontalCenter
        anchors.top: r_0006.bottom
    }
    Rectangle{ smooth: false
        id: r_0017
        width: standardRectWidth
        height: standardRectHeight
        color: "violet"
        border.width: borderSize
        border.color: "black"
        anchors.horizontalCenter: r_0007.horizontalCenter
       anchors.top: r_0007.bottom
    }
    Rectangle{ smooth: false
        id: r_0018
        width: standardRectWidth
        height: standardRectHeight
        color: "black"
        border.width: borderSize
        border.color: "dark grey"
        anchors.horizontalCenter: r_0008.horizontalCenter
        anchors.top: r_0008.bottom
    }
    Rectangle{ smooth: false
        id: r_0019
        width: standardRectWidth
        height: standardRectHeight
        color: "dark grey"
        border.width: borderSize
        border.color: "light grey"
        anchors.horizontalCenter: r_0009.horizontalCenter
        anchors.top: r_0009.bottom
    }
    Rectangle{ smooth: false
        id: r_0020
        width: standardRectWidth
        height: standardRectHeight
        color: "light grey"
        border.width: borderSize
        border.color: "red"
        anchors.horizontalCenter: r_0010.horizontalCenter
        anchors.top: r_0010.bottom
    }
    Loader{
        sourceComponent: annotation;
        property string annotationTextLabel: "BASIC COLOR BORDER";
        property int textAnnotationXOffset: 4;
        property int textAnnotationYOffset: 10 + standardRectHeight;
    }
    //Test Gradients
    Rectangle{ smooth: false
        id: r_0021
        width: standardRectWidth
        height: standardRectHeight
        color: "red"
        gradient: Gradient{
            GradientStop{ position: 1.0; color: r_0021.color }
            GradientStop{ position: 0.0; color: "white" }
        }
        anchors.horizontalCenter: r_0001.horizontalCenter
        anchors.top: r_0011.bottom
    }
    Rectangle{ smooth: false
        id: r_0022
        width: standardRectWidth
        height: standardRectHeight
        color: "orange"
        gradient: Gradient{
            GradientStop{ position: 1.0; color: r_0022.color }
            GradientStop{ position: 0.0; color: "white" }
        }
        anchors.horizontalCenter: r_0002.horizontalCenter
        anchors.top: r_0012.bottom
    }
    Rectangle{ smooth: false
        id: r_0023
        width: standardRectWidth
        height: standardRectHeight
        color: "yellow"
        gradient: Gradient{
            GradientStop{ position: 1.0; color: r_0023.color }
            GradientStop{ position: 0.0; color: "white" }
        }
        anchors.horizontalCenter: r_0003.horizontalCenter
        anchors.top: r_0013.bottom
    }
    Rectangle{ smooth: false
        id: r_0024
        width: standardRectWidth
        height: standardRectHeight
        color: "green"
        gradient: Gradient{
            GradientStop{ position: 1.0; color: r_0024.color }
            GradientStop{ position: 0.0; color: "white" }
        }
        anchors.horizontalCenter: r_0004.horizontalCenter
        anchors.top: r_0014.bottom
    }
    Rectangle{ smooth: false
        id: r_0025
        width: standardRectWidth
        height: standardRectHeight
        color: "blue"
        gradient: Gradient{
            GradientStop{ position: 1.0; color: r_0025.color }
            GradientStop{ position: 0.0; color: "white" }
        }
        anchors.horizontalCenter: r_0005.horizontalCenter
        anchors.top: r_0015.bottom
    }
    Rectangle{ smooth: false
        id: r_0026
        width: standardRectWidth
        height: standardRectHeight
        color: "indigo"
        gradient: Gradient{
            GradientStop{ position: 1.0; color: r_0026.color }
            GradientStop{ position: 0.0; color: "white" }
        }
        anchors.horizontalCenter: r_0006.horizontalCenter
        anchors.top: r_0016.bottom
    }
    Rectangle{ smooth: false
        id: r_0027
        width: standardRectWidth
        height: standardRectHeight
        color: "violet"
        gradient: Gradient{
            GradientStop{ position: 1.0; color: r_0027.color }
            GradientStop{ position: 0.0; color: "white" }
        }
        anchors.horizontalCenter: r_0007.horizontalCenter
       anchors.top: r_0017.bottom
    }
    Rectangle{ smooth: false
        id: r_0028
        width: standardRectWidth
        height: standardRectHeight
        color: "black"
        gradient: Gradient{
            GradientStop{ position: 1.0; color: r_0028.color }
            GradientStop{ position: 0.0; color: "white" }
        }
        anchors.horizontalCenter: r_0008.horizontalCenter
        anchors.top: r_0018.bottom
    }
    Rectangle{ smooth: false
        id: r_0029
        width: standardRectWidth
        height: standardRectHeight
        color: "dark grey"
        gradient: Gradient{
            GradientStop{ position: 1.0; color: r_0029.color }
            GradientStop{ position: 0.0; color: "white" }
        }
        anchors.horizontalCenter: r_0009.horizontalCenter
        anchors.top: r_0019.bottom
    }
    Rectangle{ smooth: false
        id: r_0030
        width: standardRectWidth
        height: standardRectHeight
        color: "light grey"
        gradient: Gradient{
            GradientStop{ position: 1.0; color: r_0030.color }
            GradientStop{ position: 0.0; color: "white" }
        }
        anchors.horizontalCenter: r_0010.horizontalCenter
        anchors.top: r_0020.bottom
    }
    Loader{
        sourceComponent: annotation;
        property string annotationTextLabel: "BASIC COLOR GRADIENT";
        property int textAnnotationXOffset: 4;
        property int textAnnotationYOffset: 10 + (2*standardRectHeight);
    }
    //Test radius
    Rectangle{ smooth: false
        id: r_0031
        x: 0
        y: 97
        width: r_0000.standardRectWidth
        height: r_0000.standardRectHeight
        color: "red"
        radius: 1
        anchors.horizontalCenter: r_0001.horizontalCenter
        anchors.top: r_0021.bottom
    }
    Rectangle{ smooth: false
        id: r_0032
        x: r_0031.x + standardRectWidth
        y: r_0031.y
        width: standardRectWidth
        height: standardRectHeight
        color: "orange"
        radius: 2
        anchors.horizontalCenter: r_0002.horizontalCenter
        anchors.top: r_0022.bottom
    }
    Rectangle{ smooth: false
        id: r_0033
        x: r_0032.x + standardRectWidth
        y: r_0032.y
        width: standardRectWidth
        height: standardRectHeight
        color: "yellow"
        radius: 3
        anchors.horizontalCenter: r_0003.horizontalCenter
        anchors.top: r_0023.bottom
    }
    Rectangle{ smooth: false
        id: r_0034
        x: r_0033.x + standardRectWidth
        y: r_0033.y
        width: standardRectWidth
        height: standardRectHeight
        color: "green"
        radius: 4
        anchors.horizontalCenter: r_0004.horizontalCenter
        anchors.top: r_0024.bottom
    }
    Rectangle{ smooth: false
        id: r_0035
        x: r_0034.x + standardRectWidth
        y: r_0034.y
        width: standardRectWidth
        height: standardRectHeight
        color: "blue"
        radius: 5
        anchors.horizontalCenter: r_0005.horizontalCenter
        anchors.top: r_0025.bottom
    }
    Rectangle{ smooth: false
        id: r_0036
        x: r_0035.x + standardRectWidth
        y: r_0035.y
        width: standardRectWidth
        height: standardRectHeight
        color: "indigo"
        radius: 6
        anchors.horizontalCenter: r_0006.horizontalCenter
        anchors.top: r_0026.bottom
    }
    Rectangle{ smooth: false
        id: r_0037
        x: r_0036.x + standardRectWidth
        y: r_0036.y
        width: standardRectWidth
        height: standardRectHeight
        color: "violet"
        radius: 7
        anchors.horizontalCenter: r_0007.horizontalCenter
        anchors.top: r_0027.bottom
    }
    Rectangle{ smooth: false
        id: r_0038
        x: r_0037.x + standardRectWidth
        y: r_0037.y
        width: standardRectWidth
        height: standardRectHeight
        color: "black"
        radius: 8
        anchors.horizontalCenter: r_0008.horizontalCenter
        anchors.top: r_0028.bottom
    }
    Rectangle{ smooth: false
        id: r_0039
        x: r_0038.x + standardRectWidth
        y: r_0038.y
        width: standardRectWidth
        height: standardRectHeight
        color: "dark grey"
        radius: 9
        anchors.horizontalCenter: r_0009.horizontalCenter
        anchors.top: r_0029.bottom
    }
    Rectangle{ smooth: false
        id: r_0040
        x: r_0039.x + standardRectWidth
        y: r_0039.y
        width: standardRectWidth
        height: standardRectHeight
        color: "light grey"
        radius: 10
        anchors.horizontalCenter: r_0010.horizontalCenter
        anchors.top: r_0030.bottom
    }
    Loader{
        sourceComponent: annotation;
        property string annotationTextLabel: "BASIC RADIUS";
        property int textAnnotationXOffset: 4;
        property int textAnnotationYOffset: 10 + (3*standardRectHeight);
    }

}

