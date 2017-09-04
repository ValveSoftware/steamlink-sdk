import QtQuick 2.5

Item {
    property var someProp: 1
    Component.onCompleted: console.log("typeof someProp:", typeof(someProp));
}
