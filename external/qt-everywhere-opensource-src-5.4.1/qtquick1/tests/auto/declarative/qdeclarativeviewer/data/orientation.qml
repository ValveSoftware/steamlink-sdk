import QtQuick 1.0
Rectangle {
    color: "black"
    width: (runtime.orientation == Orientation.Landscape || runtime.orientation == Orientation.LandscapeInverted) ? 300 : 200
    height: (runtime.orientation == Orientation.Landscape || runtime.orientation == Orientation.LandscapeInverted) ? 200 : 300
    Text {
        text: {
            if (runtime.orientation == Orientation.Portrait)
                return "Portrait"
            if (runtime.orientation == Orientation.PortraitInverted)
                return "PortraitInverted"
            if (runtime.orientation == Orientation.Landscape)
                return "Landscape"
            if (runtime.orientation == Orientation.LandscapeInverted)
                return "LandscapeInverted"
        }
        color: "white"
    }
}
