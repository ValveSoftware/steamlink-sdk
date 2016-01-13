import QtQuick 2.0

Item {
    property int changeCount: 0

    property bool _6nameWithUnderscoreNumber: false

    // invalid property name - the first character after an underscore must be a letter
    on_6NameWithUnderscoreNumberChanged: {
        changeCount = changeCount + 3;
    }
}
