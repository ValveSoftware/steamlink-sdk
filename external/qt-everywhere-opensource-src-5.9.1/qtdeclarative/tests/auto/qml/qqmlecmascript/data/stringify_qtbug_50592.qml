import QtQuick 2.0

Item {
    Image {
        id: img
        source: "http://example.org/some_nonexistant_image.png"
        visible: false
    }

    property string source: JSON.stringify(img.source)
}

