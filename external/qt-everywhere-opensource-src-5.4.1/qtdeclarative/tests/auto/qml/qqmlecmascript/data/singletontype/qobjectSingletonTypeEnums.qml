import QtQuick 2.0
import Qt.test.qobjectApi 1.0 as QtTestQObjectApi               // qobject singleton Type installed into new uri

QtObject {
    property int enumValue: QtTestQObjectApi.QObject.EnumValue2;
    property int enumMethod: QtTestQObjectApi.QObject.qobjectEnumTestMethod(QtTestQObjectApi.QObject.EnumValue1);
}

