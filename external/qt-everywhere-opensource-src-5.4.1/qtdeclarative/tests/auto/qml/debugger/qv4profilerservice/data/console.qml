import QtQuick 2.0


Item {
    function f()
    {
    }

    Component.onCompleted:  {
        console.profile();
        f();
        console.profileEnd();
    }
}
