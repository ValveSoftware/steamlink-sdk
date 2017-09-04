import QtQuick 2.0
import QtQuick.XmlListModel 2.0

XmlListModel {
    source: "model.xml"
    query: "/Pets/Pet"
    XmlRole { name: "name"; query: "/name/string()" }   //starts with '/'
    XmlRole { name: "type"; query: "type" }             //no type
    XmlRole { name: "age"; query: "age/" }              //ends with '/'
    XmlRole { name: "size"; query: "size/number()" }    //wrong type
}
