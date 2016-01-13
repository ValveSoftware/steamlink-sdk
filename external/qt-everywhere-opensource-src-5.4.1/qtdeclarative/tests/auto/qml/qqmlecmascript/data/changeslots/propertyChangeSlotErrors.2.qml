import QtQuick 2.0

Item {
    property int changeCount: 0

    property bool ____nameWithUnderscores: false

    // this should error, since the first alpha isn't capitalised
    on____nameWithUnderscoresChanged: {
        changeCount = changeCount + 3;
    }
}
