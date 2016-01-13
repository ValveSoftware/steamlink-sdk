import QtQuick 2.0
import QtTest 1.1

Rectangle {
  id:container
  width:50
  height:50

  Rectangle {id:rect; x:0; y:0; color:"red"; width:10; height:10}
  AnimationController {
     id:colorAnimationcontroller
     progress:1
     animation: ColorAnimation {id:anim; target: rect; property:"color"; to:"#FFFFFF"; from:"#000000"; duration: 1000}
  }

  TestCase {
    name:"AnimationController"
    when:windowShown
    function test_colorAnimation() {
      colorAnimationcontroller.progress = 0;
      compare(rect.color.toString(), "#000000");
      colorAnimationcontroller.progress = 0.5;
      compare(rect.color.toString(), "#7f7f7f");

      // <=0 -> 0
      colorAnimationcontroller.progress = -1;
      compare(rect.color, "#000000");

      //>=1 -> 1
      colorAnimationcontroller.progress = 1.1;
      compare(rect.color.toString(), "#ffffff");

      //make sure the progress can be set backward
      colorAnimationcontroller.progress = 0.5;
      compare(rect.color, "#7f7f7f");
    }
  }
}