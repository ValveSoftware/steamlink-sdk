import QtQuick 2.0

CanvasTestCase {
   id:testCase
   name: "strokeStyle"
   function init_data() { return testData("2d"); }
   function test_default(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       compare(ctx.strokeStyle, "#000000")
       ctx.clearRect(0, 0, 1, 1);
       compare(ctx.strokeStyle, "#000000")
       canvas.destroy()
   }
   function test_saverestore(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       var old = ctx.strokeStyle;
       ctx.save();
       ctx.strokeStyle = "#ffaaff";
       ctx.restore();
       compare(ctx.strokeStyle, old);

       ctx.strokeStyle = "#ffcc88";
       old = ctx.strokeStyle;
       ctx.save();
       compare(ctx.strokeStyle, old);
       ctx.restore();
       canvas.destroy()
   }
   function test_namedColor(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       ctx.strokeStyle = "red";
       ctx.strokeRect(0,0,1,1);
       comparePixel(ctx,0,0,255,0,0,255);

       ctx.strokeStyle = "black";
       ctx.strokeRect(0,0,1,1);
       comparePixel(ctx,0,0,0,0,0,255);

       ctx.strokeStyle = "white";
       ctx.strokeRect(0,0,1,1);
       comparePixel(ctx,0,0,255,255,255,255);
       canvas.destroy()
   }
}
