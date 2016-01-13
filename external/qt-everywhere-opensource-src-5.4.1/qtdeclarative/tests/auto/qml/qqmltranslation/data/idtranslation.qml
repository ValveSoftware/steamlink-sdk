import QtQuick 2.0

QtObject {
    property string _idTranslation2: QT_TRID_NOOP("qtn_hello_world")
    property string idTranslation: qsTrId("qtn_hello_world")
    property string idTranslation2: qsTrId(_idTranslation2)
    property string idTranslation3: if (1) qsTrId("qtn_hello_world")
}
