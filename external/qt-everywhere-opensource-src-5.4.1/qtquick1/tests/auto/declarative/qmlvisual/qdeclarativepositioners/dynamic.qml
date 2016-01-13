import QtQuick 1.0

Item {
    property string skip: "Expected to fail until QTBUG-14839 is resolved"
    width: 120; height: 60;
    property int step: 0
    function tick()
    {
        step++;
        if(step == 1){
            //row1.destroy(); //Not dynamically created, so is this valid?
        }else if(step == 2){
            r2a.destroy();
        }else if(step == 3){
            r2b.destroy();
        }else if(step == 4){
            r2c.destroy();
        }else if(step == 5){
            r3a.parent = row2;
        }else if(step == 6){
            r3b.parent = row2;
        }else if(step == 7){
            r3c.parent = row2;
        }else if(step == 8){
            //row3.destroy();//empty now, so should have no effect//May be invalid, but was deleting the reparent items at one point
        }else{
            repeater.model++;
        }
    }

    //Tests base positioner functionality, so don't need them all.
    Column{
        move: Transition{NumberAnimation{properties:"y"}}
        Row{
            id: row1
            height: childrenRect.height
            Rectangle{id: r1a; width:20; height:20; color: "red"}
            Rectangle{id: r1b; width:20; height:20; color: "green"}
            Rectangle{id: r1c; width:20; height:20; color: "blue"}
        }
        Row{
            id: row2
            height: childrenRect.height
            move: Transition{NumberAnimation{properties:"x"}}
            Repeater{
                id: repeater
                model: 0;
                delegate: Component{ Rectangle { color: "yellow"; width:20; height:20;}}
            }
            Rectangle{id: r2a; width:20; height:20; color: "red"}
            Rectangle{id: r2b; width:20; height:20; color: "green"}
            Rectangle{id: r2c; width:20; height:20; color: "blue"}
        }
        Row{
            move: Transition{NumberAnimation{properties:"x"}}
            id: row3
            height: childrenRect.height
            Rectangle{id: r3a; width:20; height:20; color: "red"}
            Rectangle{id: r3b; width:20; height:20; color: "green"}
            Rectangle{id: r3c; width:20; height:20; color: "blue"}
        }
    }
    Timer{
        interval: 250;
        running: true;
        repeat: true;
        onTriggered: tick();
    }
}

