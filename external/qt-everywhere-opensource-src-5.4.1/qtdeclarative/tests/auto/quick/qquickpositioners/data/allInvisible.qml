import QtQuick 2.0

Item{
    width: 400
    height: 400
    Column{
        spacing: 20
        objectName: "column"
        Item{
            width: 0
            height: 20
            visible: false
        }
        Item{
            width: 20
            height: 0
            visible: false
        }
        Item{
            width: 20
            height: 20
            visible: false
        }
    }
    Row{
        spacing: 20
        objectName: "row"
        Item{
            width: 0
            height: 20
            visible: false
        }
        Item{
            width: 20
            height: 0
            visible: false
        }
        Item{
            width: 20
            height: 20
            visible: false
        }
    }
}
