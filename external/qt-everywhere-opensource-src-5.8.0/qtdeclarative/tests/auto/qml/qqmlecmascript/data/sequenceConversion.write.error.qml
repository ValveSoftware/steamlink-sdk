import QtQuick 2.0
import Qt.test 1.0

Item {
    id: root
    objectName: "root"

    MySequenceConversionObject {
        id: msco
        objectName: "msco"
    }

    function performTest() {
        // we have NOT registered QList<QPoint> as a type
        var pointList = [ Qt.point(7,7), Qt.point(8,8), Qt.point(9,9) ];
        msco.pointListProperty = pointList; // error.
    }
}
