import QtQuick 2.0
import QtTest 1.1

CanvasTestCase {
   id:testCase
   name: "pixel"
   function init_data() { return testData("2d"); }
   function test_createImageData(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       var imageData = ctx.createImageData(1, 1);
       var imageDataValues = imageData.data;
       imageDataValues[0] = 255;
       imageDataValues[0] = 0;
       if (imageDataValues[0] != 0)
           qtest_fail('ImageData value access fail, expecting 0, got ' + imageDataValues[0]);

       ctx.reset();
       canvas.destroy()
  }
   function test_getImageData(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       canvas.destroy()
  }
   function test_object(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       canvas.destroy()
  }
   function test_putImageData(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       canvas.destroy()
  }
   function test_filters(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       canvas.destroy()
  }
}
