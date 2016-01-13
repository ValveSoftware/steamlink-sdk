import QtQuick 1.0

Item{
    width: 40; height: 320
    Column{
        Rectangle{color:"Red"; width:40; height:40;}
        Repeater{
            id: rep
            model: 3
            delegate: Component{Rectangle{color:"Green"; width:40; height:40; radius: 20;}}
        }
        Rectangle{color:"Blue"; width:40; height:40;}
    }
    Timer{ interval: 250; running: true; onTriggered: rep.model=6;}
    Timer{ interval: 500; running: true; onTriggered: Qt.quit();}
}
