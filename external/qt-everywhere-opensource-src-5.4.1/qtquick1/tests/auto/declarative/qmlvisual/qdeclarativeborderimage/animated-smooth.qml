import QtQuick 1.0
import "content"

Rectangle {
    id: page
    color: "white"
    width: 520; height: 260
    Grid{
        columns: 4
        spacing: 4
        MyBorderImage {
            minWidth: 60; maxWidth: 120
            minHeight: 60; maxHeight: 120
            source: "content/colors.png"; margin: 15
            antialiased: true
        }
        MyBorderImage {
            minWidth: 60; maxWidth: 120
            minHeight: 60; maxHeight: 120
            source: "content/colors.png"; margin: 15
            horizontalMode: BorderImage.Repeat; verticalMode: BorderImage.Repeat
            antialiased: true
        }
        MyBorderImage {
            minWidth: 60; maxWidth: 120
            minHeight: 60; maxHeight: 120
            source: "content/colors.png"; margin: 15
            horizontalMode: BorderImage.Stretch; verticalMode: BorderImage.Repeat
            antialiased: true
        }
        MyBorderImage {
            minWidth: 60; maxWidth: 120
            minHeight: 60; maxHeight: 120
            source: "content/colors.png"; margin: 15
            horizontalMode: BorderImage.Round; verticalMode: BorderImage.Round
            antialiased: true
        }
        MyBorderImage {
            minWidth: 60; maxWidth: 120
            minHeight: 40; maxHeight: 120
            source: "content/bw.png"; margin: 10
            antialiased: true
        }
        MyBorderImage {
            minWidth: 60; maxWidth: 120
            minHeight: 40; maxHeight: 120
            source: "content/bw.png"; margin: 10
            horizontalMode: BorderImage.Repeat; verticalMode: BorderImage.Repeat
            antialiased: true
        }
        MyBorderImage {
            minWidth: 60; maxWidth: 120
            minHeight: 40; maxHeight: 120
            source: "content/bw.png"; margin: 10
            horizontalMode: BorderImage.Stretch; verticalMode: BorderImage.Repeat
            antialiased: true
        }
        MyBorderImage {
            minWidth: 60; maxWidth: 120
            minHeight: 40; maxHeight: 120
            source: "content/bw.png"; margin: 10
            horizontalMode: BorderImage.Round; verticalMode: BorderImage.Round
            antialiased: true
        }
    }
}
