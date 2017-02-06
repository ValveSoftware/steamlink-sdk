import QtQuick 2.0
import QtQuick.XmlListModel 2.0
import SortFilterProxyModel 1.0

SortFilterProxyModel {
    source: XmlListModel {
        XmlRole { }
    }
}
