import QtQuick 2.0
import QtQuick.XmlListModel 2.0

XmlListModel {
    source: "groups.xml"
    query: "//animal[@name='Garfield']/parent::group"

    XmlRole { name: "id"; query: "@id/string()" }
    XmlRole { name: "name"; query: "@name/string()" }
}
