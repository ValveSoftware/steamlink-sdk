import QtQuick 2.0

// this used to trigger crash: see QTBUG-23126
Item {
    id: root
    property bool success: false

    // each of these property names have partial strings
    // which are prehashed by v8 (random, cos, sin, ...)
    property int randomProperty: 4
    property real cosProperty: 1
    property real cossin: 1
    property real propertycos: 1
    property real cos: 1

    // these property names are entirely "new".
    property string kqwpald: "hello"
    property bool poiuyt: false

    Component.onCompleted: {
        success = true;

        // 1: ensure that we can access the properties by name

        if (root["random" + "Property"] != 4) {
            success = false;
            return;
        }

        if (root["cos" + "Property"] != 1) {
            success = false;
            return;
        }

        if (root["cos" + "sin"] != 1) {
            success = false;
            return;
        }

        if (root["property" + "cos"] != 1) {
            success = false;
            return;
        }

        if (root["" + "cos"] != 1) {
            success = false;
            return;
        }

        if (root["cos" + ""] != 1) {
            success = false;
            return;
        }

        if (root["kq" + "wpald"] != "hello") {
            success = false;
            return;
        }

        if (root["poiu" + "yt"] != false) {
            success = false;
            return;
        }

        // 2: ensure that similarly named properties don't cause crash

        if (root["random" + "property"] == 4) {
            success = false;
            return;
        }

        if (root["cos" + "property"] == 1) {
            success = false;
            return;
        }

        if (root["cos" + "Sin"] == 1) {
            success = false;
            return;
        }

        if (root["property" + "Cos"] == 1) {
            success = false;
            return;
        }

        if (root["" + "Cos"] == 1) {
            success = false;
            return;
        }

        if (root["Cos" + ""] == 1) {
            success = false;
            return;
        }

        if (root["kq" + "Wpald"] == "hello") {
            success = false;
            return;
        }

        if (root["poiu" + "Yt"] == false) {
            success = false;
            return;
        }

        // 3: ensure that toString doesn't crash

        root["random" + "Property"].toString();
        root["cos" + "Property"].toString();
        root["cos" + "Sin"] ? root["cos" + "Sin"].toString() : "";
        root["property" + "Cos"] ? root["property" + "Cos"].toString() : "";
        root["Cos" + ""] ? root["Cos" + ""].toString() : "";
        root["" + "Cos"] ? root["" + "Cos"].toString() : "";
        root["kq" + "Wpald"] ? root["kq" + "Wpald"].toString() : "";
        root["poiu" + "Yt"] ? root["poiu" + "Yt"].toString() : "";

        root["random" + "property"] ? root["random" + "property"].toString() : "";
        root["cos" + "property"] ? root["cos" + "property"].toString() : "";
        root["cos" + "sin"] ? root["cos" + "sin"].toString() : "";
        root["property" + "cos"] ? root["property" + "cos"].toString() : "";
        root["cos" + ""] ? root["cos" + ""].toString() : "";
        root["" + "cos"] ? root["" + "cos"].toString() : "";
        root["kq" + "wpald"] ? root["kq" + "wpald"].toString() : "";
        root["poiu" + "yt"] ? root["poiu" + "yt"].toString() : "";
    }
}
