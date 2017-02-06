import QtQuick 2.0
import QtTest 1.1
import QtQuick.Window 2.1

TestCase {
  id:testCase
  when:windowShown
  width:100
  height:100
  property Component component:CanvasComponent{}
  function cleanupTestCase() {
    wait(100) //wait for a short while to make sure no leaked textures
  }

  function testData(type) {
    if (type === "2d")
      return [
             { tag:"image threaded", properties:{width:100, height:100, renderTarget:Canvas.Image, renderStrategy:Canvas.Threaded}},
//             { tag:"image cooperative", properties:{width:100, height:100, renderTarget:Canvas.Image, renderStrategy:Canvas.Cooperative}},
             { tag:"image immediate", properties:{width:100, height:100, renderTarget:Canvas.Image, renderStrategy:Canvas.Immediate}},
//             { tag:"fbo cooperative", properties:{width:100, height:100, renderTarget:Canvas.FramebufferObject, renderStrategy:Canvas.Cooperative}},
             { tag:"fbo immediate", properties:{width:100, height:100, renderTarget:Canvas.FramebufferObject, renderStrategy:Canvas.Immediate}},
             { tag:"fbo threaded", properties:{width:100, height:100, renderTarget:Canvas.FramebufferObject, renderStrategy:Canvas.Threaded}}
           ];
     return [];
  }

  function renderStrategyToString(renderStrategy) {
      return renderStrategy === Canvas.Immediate ? "Canvas.Immediate" :
            (renderStrategy === Canvas.Threaded ? "Canvas.Threaded" : "Canvas.Cooperative");
  }

  function createCanvasObject(data) {
      var canvas = component.createObject(testCase, data.properties);
      waitForRendering(canvas);
      return canvas;
  }

  function comparePixel(ctx,x,y,r,g,b,a, d)
  {
    var c = ctx.getImageData(x,y,1,1).data;
    if (d === undefined)
      d = 0;
    r = Math.round(r);
    g = Math.round(g);
    b = Math.round(b);
    a = Math.round(a);

    var notSame = Math.abs(c[0]-r)>d || Math.abs(c[1]-g)>d || Math.abs(c[2]-b)>d || Math.abs(c[3]-a)>d;
    if (notSame)
      qtest_fail('Pixel compare fail:\nactual  :[' + c[0]+','+c[1]+','+c[2]+','+c[3] + ']\nexpected:['+r+','+g+','+b+','+a+'] +/- '+d, 1);
  }

}
