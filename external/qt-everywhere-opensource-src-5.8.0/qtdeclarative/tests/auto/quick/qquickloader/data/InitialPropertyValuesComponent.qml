import QtQuick 2.0

Item {
    id: behaviorCounter
    property int behaviorCount: 0
    property int canary: 0

    Behavior on canary {
        NumberAnimation { target: behaviorCounter; property: "behaviorCount"; to: (behaviorCounter.behaviorCount + 1); duration: 0 }
    }
}
