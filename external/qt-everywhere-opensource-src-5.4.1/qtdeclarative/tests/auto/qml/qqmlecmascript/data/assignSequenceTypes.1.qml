import QtQuick 2.0
import Qt.test 1.0

MySequenceConversionObject {
    intListProperty: [1, 2]
    qrealListProperty: [1.1, 2.2, 3]
    boolListProperty: [false, true]
    urlListProperty: [ "http://www.example1.com", "http://www.example2.com" ]
    stringListProperty: [ "one", "two" ]
    qstringListProperty: [ "one", "two" ]
}
