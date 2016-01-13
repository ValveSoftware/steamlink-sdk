import QtQuick 2.0

Rectangle {
    Image {
        source: "TestImage_2x2.png"
        onStatusChanged: switch (status) {
                           case 0: console.log("no image"); break;
                           case 1: console.log("image loaded"); break;
                           case 2: console.log("image loading"); break;
                           case 3: console.log("image error"); break;
                         }
    }
}
