import QtQuick 1.0

Item {
    width: 640
    height: 480
    Column {
        objectName: "column"
        QGraphicsWidget {
            objectName: "one"
            width: 50
            height: 50
        }
        QGraphicsWidget {
            objectName: "two"
            width: 20
            height: 10
        }
        QGraphicsWidget {
            objectName: "three"
            width: 40
            height: 20
        }
    }
}
