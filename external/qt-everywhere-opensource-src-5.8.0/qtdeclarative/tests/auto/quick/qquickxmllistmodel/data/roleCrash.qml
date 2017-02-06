import QtQuick 2.0
import QtQuick.XmlListModel 2.0

XmlListModel {
    id: model
    XmlRole {}
    Component.onCompleted: model.roles = 0
}
