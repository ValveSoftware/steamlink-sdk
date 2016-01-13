import QtQuick 2.0

Item {
    property int changeCount: 0

    property bool _nameWithUnderscore: false

    // this should error, since the first alpha isn't capitalised.
    on_nameWithUnderscoreChanged: {
        changeCount = changeCount + 2;
    }
}
