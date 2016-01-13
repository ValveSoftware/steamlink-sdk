import QtQuick 2.1
import QtTest 1.1

Item {
    id: top

    PathView {
        id: pathViewDelegateModelAfterCreate
        anchors.fill: parent
        property int createdDelegates: 0
        path: Path { startX: 120; startY: 100 }
    }

    Component {
        id: delegateModelAfterCreateComponent
        Rectangle {
            width: 140
            height: 140
            border.color: "black"
            color: "red"
            Component.onCompleted: pathViewDelegateModelAfterCreate.createdDelegates++;
        }
    }

    TestCase {
        name: "PathView"
        when: windowShown

        function test_set_delegate_model_after_path_creation() {
            pathViewDelegateModelAfterCreate.delegate = delegateModelAfterCreateComponent;
            pathViewDelegateModelAfterCreate.model = 40;
            verify(pathViewDelegateModelAfterCreate.createdDelegates > 0);
        }
    }
}
