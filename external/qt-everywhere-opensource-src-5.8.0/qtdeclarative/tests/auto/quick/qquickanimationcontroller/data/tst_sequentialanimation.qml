import QtQuick 2.0
import QtTest 1.1

Rectangle {
  id:container
  width:100
  height:100

  Rectangle {id:rect; x:0; y:0; color:"red"; width:10; height:10}
  AnimationController {
     id:controller
     progress:0
     animation: SequentialAnimation {
       id:anim
       NumberAnimation { target: rect; property: "x"; from:0; to: 50; duration: 1000 }
       NumberAnimation { target: rect; property: "y"; from:0; to: 100; duration: 1000 }
       NumberAnimation { target: rect; property: "height"; from:10; to: 50; duration: 1000 }
       NumberAnimation { target: rect; property: "width"; from:10; to: 50; duration: 1000 }
       ColorAnimation {target:rect; property:"color"; from:"red"; to:"blue"; duration:1000 }
     }
  }


  TestCase {
    name:"AnimationController"
    when:windowShown
    function test_sequentialAnimation_data() {
       return [
           {tag:"0.1",progress:0.1, x:25, y:0, color:"#ff0000", width:10, height:10},
           {tag:"0.2",progress:0.2, x:50, y:0, color:"#ff0000", width:10, height:10},
           {tag:"0.30000000000000004",progress:0.30000000000000004, x:50, y:50, color:"#ff0000", width:10, height:10},
           {tag:"0.4",progress:0.4, x:50, y:100, color:"#ff0000", width:10, height:10},
           {tag:"0.5",progress:0.5, x:50, y:100, color:"#ff0000", width:10, height:30},
           {tag:"0.6",progress:0.5999999999999, x:50, y:100, color:"#ff0000", width:10, height:49.96},
           {tag:"0.7",progress:0.6999999999999, x:50, y:100, color:"#ff0000", width:29.96, height:50},
           {tag:"0.7999999999999999",progress:0.7999999999999999, x:50, y:100, color:"#ff0000", width:49.96, height:50},
           {tag:"0.8999999999999999",progress:0.8999999999999999, x:50, y:100, color:"#7f007f", width:50, height:50},
           {tag:"0.9999999999999999",progress:0.9999999999999999, x:50, y:100, color:"#0000fe", width:50, height:50},
           {tag:"1",progress:1, x:50, y:100, color:"#0000ff", width:50, height:50},
           {tag:"0.9",progress:0.9, x:50, y:100, color:"#7f007f", width:50, height:50},
           {tag:"0.8",progress:0.8, x:50, y:100, color:"#ff0000", width:50, height:50},
           {tag:"0.7000000000000001",progress:0.7000000000000001, x:50, y:100, color:"#ff0000", width:30, height:50},
           {tag:"0.6000000000000001",progress:0.6000000000000001, x:50, y:100, color:"#ff0000", width:10, height:50},
           {tag:"0.5000000000000001",progress:0.5000000000000001, x:50, y:100, color:"#ff0000", width:10, height:30},
           {tag:"0.40000000000000013",progress:0.40000000000000013, x:50, y:100, color:"#ff0000", width:10, height:10},
           {tag:"0.30000000000000016",progress:0.30000000000000016, x:50, y:50, color:"#ff0000", width:10, height:10},
           {tag:"0.20000000000000015",progress:0.20000000000000015, x:50, y:0, color:"#ff0000", width:10, height:10},
           {tag:"0.10000000000000014",progress:0.10000000000000014, x:25, y:0, color:"#ff0000", width:10, height:10},
           {tag:"1.3877787807814457e-16",progress:1.3877787807814457e-16, x:0, y:0, color:"#ff0000", width:10, height:10},
           {tag:"0",progress:0, x:0, y:0, color:"#ff0000", width:10, height:10},
           {tag:"0.1",progress:0.1, x:25, y:0, color:"#ff0000", width:10, height:10},
           {tag:"0.2",progress:0.2, x:50, y:0, color:"#ff0000", width:10, height:10}

              ];
    }
    function test_sequentialAnimation(row) {
      controller.progress = row.progress;
      compare(rect.x, row.x);
      compare(rect.y, row.y);
      compare(rect.width, row.width);
      compare(rect.height, row.height);
      compare(rect.color.toString(), row.color);
    }
  }
}
