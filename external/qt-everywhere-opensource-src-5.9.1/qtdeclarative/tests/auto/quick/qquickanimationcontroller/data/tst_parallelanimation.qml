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
     animation: ParallelAnimation {
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
    function test_parallelAnimation_data() {
       //FIXME:the commented lines fail on OS X
       return [
              {tag:"0.1",progress:0.1, x:5, y:10, color:"#e50019", width:14, height:14},
              //{tag:"0.2",progress:0.2, x:10, y:20, color:"#cb0033", width:18, height:18},
              {tag:"0.30000000000000004",progress:0.30000000000000004, x:15, y:30, color:"#b2004c", width:22, height:22},
              //{tag:"0.4",progress:0.4, x:20, y:40, color:"#980066", width:26, height:26},
              {tag:"0.5",progress:0.5, x:25, y:50, color:"#7f007f", width:30, height:30},
              {tag:"0.6",progress:0.59999999, x:29.95, y:59.9, color:"#660098", width:33.96, height:33.96},
              {tag:"0.7",progress:0.69999999, x:34.949999999999996, y:69.89999999999999, color:"#4c00b2", width:37.96, height:37.96},
              {tag:"0.7999999999999999",progress:0.7999999999999999, x:39.95, y:79.9, color:"#3300cb", width:41.96, height:41.96},
              {tag:"0.8999999999999999",progress:0.8999999999999999, x:44.95, y:89.9, color:"#1900e5", width:45.96, height:45.96},
              {tag:"0.9999999999999999",progress:0.9999999999999999, x:49.95, y:99.9, color:"#0000fe", width:49.96, height:49.96},
              {tag:"1",progress:1, x:50, y:100, color:"#0000ff", width:50, height:50},
              {tag:"0.9",progress:0.9, x:45, y:90, color:"#1900e5", width:46, height:46},
              //{tag:"0.8",progress:0.8, x:40, y:80, color:"#3200cc", width:42, height:42},
              {tag:"0.7000000000000001",progress:0.7000000000000001, x:35, y:70, color:"#4c00b2", width:38, height:38},
              //{tag:"0.6000000000000001",progress:0.6000000000000001, x:30, y:60, color:"#660098", width:34, height:34},
              {tag:"0.5000000000000001",progress:0.5000000000000001, x:25, y:50, color:"#7f007f", width:30, height:30},
              //{tag:"0.40000000000000013",progress:0.40000000000000013, x:20, y:40, color:"#980066", width:26, height:26},
              {tag:"0.30000000000000016",progress:0.30000000000000016, x:15, y:30, color:"#b2004c", width:22, height:22},
              //{tag:"0.20000000000000015",progress:0.19999999999999999, x:10, y:20, color:"#cb0033", width:18, height:18},
              {tag:"0.10000000000000014",progress:0.10000000000000014, x:5, y:10, color:"#e50019", width:14, height:14},
              {tag:"1.3877787807814457e-16",progress:1.3877787807814457e-16, x:0, y:0, color:"#ff0000", width:10, height:10},
              {tag:"0",progress:0, x:0, y:0, color:"#ff0000", width:10, height:10},
              {tag:"0.1",progress:0.1, x:5, y:10, color:"#e50019", width:14, height:14}
              ];
    }
    function test_parallelAnimation(row) {
      controller.progress = row.progress;
      compare(rect.x, row.x);
      compare(rect.y, row.y);
      compare(rect.width, row.width);
      compare(rect.height, row.height);
      compare(rect.color.toString(), row.color);
    }
  }
}
