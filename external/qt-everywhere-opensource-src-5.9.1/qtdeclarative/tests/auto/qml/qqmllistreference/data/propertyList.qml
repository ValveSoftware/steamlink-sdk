import QtQuick 2.7

Item {
    id:root

    Component.onCompleted : {
        var st1 = Qt.createQmlObject( "import QtQuick 2.7; State{ name: 'MyState1' }", root, "dynamicState1" );
        var st2 = Qt.createQmlObject( "import QtQuick 2.7; State{ name: 'MyState2' }", root, "dynamicState2" );
        root.states.push( st1, st2 );
        root.state = "MyState2";
    }
}
