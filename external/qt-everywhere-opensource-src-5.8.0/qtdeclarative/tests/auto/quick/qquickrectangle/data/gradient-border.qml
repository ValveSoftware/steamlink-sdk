import QtQuick 2.0

Item {
    Rectangle {
        width: 100
        height: 100
        border.width: 10
        radius: 10

        gradient: Gradient {
            GradientStop {
                position: 0.00
                color: '#ffffff'
            }
            GradientStop {
                position: 0.94
                color: '#000000'
            }
        }
    }
}
