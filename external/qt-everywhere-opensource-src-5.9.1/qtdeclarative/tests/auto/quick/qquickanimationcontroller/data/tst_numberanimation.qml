import QtQuick 2.0
import QtTest 1.1

Rectangle {
  id:container
  width:50
  height:50

  Rectangle {id:rect; x:0; y:0; color:"red"; width:10; height:10}
  AnimationController {
     id:numberAnimationcontroller
     progress:1
     animation: NumberAnimation {target: rect; property: "x"; from:0; to:40; duration: 1000}
  }

  TestCase {
    name:"AnimationController"
    when:windowShown
    function test_numberAnimation() {
      numberAnimationcontroller.progress = 0;
      compare(rect.x, 0);
      numberAnimationcontroller.progress = 0.5;
      compare(rect.x, 20);

      // <=0 -> 0
      numberAnimationcontroller.progress = -1;
      compare(rect.x, 0);

      //>=1 -> 1
      numberAnimationcontroller.progress = 1.1;
      compare(rect.x, 40);

      //make sure the progress can be set backward
      numberAnimationcontroller.progress = 0.5;
      compare(rect.x, 20);
    }
  }
}