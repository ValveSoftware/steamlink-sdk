import QtQuick 2.0


Flickable {

    id: f1
    width: 320
    height: 480

    contentWidth: 360
    contentHeight: 1000

    property int cumulativeX;
    property int cumulativeY;

    function changeLabel(obj,txt){
        obj.item.lbl = txt
    }
    function changeTileMode(obj,val,mode){
        if (mode == "h")
            obj.item.hTileMode = val;
        else
            obj.item.vTileMode = val;
    }

    Component{
        id: borderImageComponent
        SimpleNoBorder{
        }
    }
    Column {
        x: 20
        y: 20
        spacing: 30
        Row {
            Text{
                id: topLabel
                text: "Border Images with no borders set"
                font.family: "Arial"
                font.pointSize: 12
            }
        }

        Row {
            spacing: 20
            Item{
                id: image_0001
                width: 70
                height: 70
                Loader{ id: ldr1; sourceComponent: borderImageComponent }
                Component.onCompleted: {
                    changeLabel(ldr1,"H: Stretch")
                    changeTileMode(ldr1,BorderImage.Stretch,"h")
                    ldr1.item.hTileMode = BorderImage.Stretch
                }
            }
            Item{
                id: image_0002
                width: 70
                height: 70
                Loader{ id: ldr2; sourceComponent: borderImageComponent }
                Component.onCompleted: {
                    changeLabel(ldr2,"H: Repeat")
                    changeTileMode(ldr2,BorderImage.Repeat,"h")
                }
            }
            Item{

                id: image_0003
                width: 70
                height: 70
                Loader{ id: ldr3; sourceComponent: borderImageComponent }
                Component.onCompleted: {
                    changeLabel(ldr3,"H: Round")
                    changeTileMode(ldr1,BorderImage.Round,"h")
                }
            }
        }
        Row {
            spacing: 20
            Item{

                id: image_0004
                width: 70
                height: 70
                Loader{ id: ldr4; sourceComponent: borderImageComponent }
                Component.onCompleted: {
                    changeLabel(ldr4,"V: Stretch")
                    changeTileMode(ldr4,BorderImage.Stretch,"v")
                }
            }
            Item{
                id: image_0005
                width: 70
                height: 70
                Loader{ id: ldr5; sourceComponent: borderImageComponent }
                Component.onCompleted: {
                    changeLabel(ldr5,"V: Repeat")
                    changeTileMode(ldr5,BorderImage.Repeat,"v")
                }
            }
            Item{
                id: image_0006
                width: 70
                height: 70
                Loader{ id: ldr6; sourceComponent: borderImageComponent }
                Component.onCompleted: {
                    changeLabel(ldr6,"H: Round")
                    changeTileMode(ldr6,BorderImage.Round,"v")
                }
            }
        }
        Row {
            spacing: 20

            Item{
                id: image_0007
                width: 70
                height: 70
                Loader{ id: ldr7; sourceComponent: borderImageComponent }
                Component.onCompleted: {
                    changeLabel(ldr7,"H/V: Stretch")
                    changeTileMode(ldr7,BorderImage.Stretch,"v")
                    changeTileMode(ldr7,BorderImage.Stretch,"h")
                }
            }
            Item{
                id: image_0008
                width: 70
                height: 70
                Loader{ id: ldr8; sourceComponent: borderImageComponent }
                Component.onCompleted: {
                    changeLabel(ldr8,"H/V: Repeat")
                    changeTileMode(ldr8,BorderImage.Repeat,"v")
                    changeTileMode(ldr8,BorderImage.Repeat,"h")
                }
            }
            Item{
                id: image_0009
                width: 70
                height: 70
                Loader{ id: ldr9; sourceComponent: borderImageComponent }
                Component.onCompleted: {
                    changeLabel(ldr9,"H/V: Round")
                    changeTileMode(ldr9,BorderImage.Round,"v")
                    changeTileMode(ldr9,BorderImage.Round,"h")
                }
            }
        }
        Row {
            spacing: 20

            Item{
                id: image_0010
                width: 70
                height: 70
                Loader{ id: ldr10; sourceComponent: borderImageComponent }
                Component.onCompleted: {
                    changeLabel(ldr10,"H/V: Stretch\nsmooth")
                    changeTileMode(ldr10,BorderImage.Stretch,"v")
                    changeTileMode(ldr10,BorderImage.Stretch,"h")
                    ldr10.item.smoothing = true
                }
            }
            Item{
                id: image_0011
                width: 70
                height: 70
                Loader{ id: ldr11; sourceComponent: borderImageComponent }
                Component.onCompleted: {
                    changeLabel(ldr11,"H/V: Repeat\nsmooth")
                    changeTileMode(ldr11,BorderImage.Repeat,"v")
                    changeTileMode(ldr11,BorderImage.Repeat,"h")
                    ldr11.item.smoothing = true
                }
            }
            Item{
                id: image_0012
                width: 70
                height: 70
                Loader{ id: ldr12; sourceComponent: borderImageComponent }
                Component.onCompleted: {
                    changeLabel(ldr12,"H/V: Round\nsmooth")
                    changeTileMode(ldr12,BorderImage.Round,"v")
                    changeTileMode(ldr12,BorderImage.Round,"h")
                    ldr10.item.smoothing = true
                }
            }
        }
    }
}
