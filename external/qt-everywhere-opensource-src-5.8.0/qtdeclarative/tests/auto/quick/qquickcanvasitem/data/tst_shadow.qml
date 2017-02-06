import QtQuick 2.0

CanvasTestCase {
   id:testCase
   name: "shadow"
   function init_data() { return testData("2d"); }
   function test_basic(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       canvas.destroy()
  }
   function test_blur(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       canvas.destroy()
   }

   function test_clip(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       canvas.destroy()
   }

   function test_composite(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       canvas.destroy()
   }

   function test_enable(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       canvas.destroy()
   }

   function test_gradient(row) {
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
   function test_offset(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       canvas.destroy()
   }
   function test_pattern(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       canvas.destroy()
   }
   function test_stroke(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       canvas.destroy()
   }
   function test_tranform(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       canvas.destroy()
   }
}
