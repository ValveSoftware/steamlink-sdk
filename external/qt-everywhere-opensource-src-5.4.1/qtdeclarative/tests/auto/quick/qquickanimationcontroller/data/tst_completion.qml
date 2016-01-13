import QtQuick 2.0
import QtTest 1.1

Rectangle {
  id:container
  width:110
  height:40

  Rectangle {id:rect; x:0; y:0; color:"red"; width:10; height:10}
  AnimationController {
     id:ctrl
     progress:0
     animation: NumberAnimation {id:anim; target: rect; property:"x"; to:100; from:0; duration: 500}
  }

  TestCase {
    name:"AnimationController"
    when:windowShown
    function test_complete() {
      skip("QTBUG-25967")

      ctrl.progress = 0;
      compare(rect.x, 0);
      ctrl.progress = 0.5;
      compare(rect.x, 50);

      ctrl.completeToBeginning();
      wait(200);
      verify(ctrl.progress < 0.5 && ctrl.progress > 0);
      wait(600);
      compare(ctrl.progress, 0);
      compare(rect.x, 0);

      ctrl.progress = 0.5;
      compare(rect.x, 50);

      ctrl.completeToEnd();
      wait(200);
      verify(ctrl.progress > 0.5 && ctrl.progress < 1);
      wait(600);
      compare(ctrl.progress, 1);
      compare(rect.x, 100);
    }
  }
}
