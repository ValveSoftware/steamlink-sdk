import QtQuick 2.0

CanvasTestCase {
   id:testCase
   name: "pattern"
   function init_data() { return testData("2d"); }
   function test_basic(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       canvas.destroy()
  }
   function test_animated(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       canvas.destroy()
  }
   function test_image(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       canvas.destroy()
  }
   function test_modified(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       canvas.destroy()
  }
   function test_paint(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       canvas.destroy()
  }
   function test_repeat(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       canvas.destroy()
  }
}
