import QtQuick 2.0

CanvasTestCase {
   id:testCase
   name: "fillStyle"
   function init_data() { return testData("2d"); }
   function test_default(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       verify(ctx.fillStyle, "#000000");
       ctx.clearRect(0, 0, 1, 1);
       compare(ctx.fillStyle, "#000000");
   }
   function test_get(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       ctx.fillStyle = '#fa0';
       compare(ctx.fillStyle, '#ffaa00');
       ctx.fillStyle = Qt.rgba(0,0,0,0);
       compare(ctx.fillStyle, 'rgba(0, 0, 0, 0.0)');
   }
   function test_hex(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       ctx.fillStyle = '#f00';
       compare(ctx.fillStyle, '#ff0000');
       ctx.fillStyle = "#0f0";
       compare(ctx.fillStyle, '#00ff00');
       ctx.fillStyle = "#0fF";
       compare(ctx.fillStyle, '#00ffff');
       ctx.fillStyle = "#0aCCfb";
       compare(ctx.fillStyle, '#0accfb');

   }
   function test_invalid(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       ctx.fillStyle = '#fa0';
       compare(ctx.fillStyle, '#ffaa00');
       ctx.fillStyle = "invalid";
       compare(ctx.fillStyle, '#ffaa00');
       ctx.fillStyle = "rgb (1, 2, 3)";
       compare(ctx.fillStyle, '#ffaa00');
       ctx.fillStyle = '#fa0';

       ctx.fillStyle = "rgba(3, 1, 2)";
       compare(ctx.fillStyle, '#ffaa00');
       ctx.fillStyle = "rgb((3,4,1)";
       compare(ctx.fillStyle, '#ffaa00');
       ctx.fillStyle = "rgb(1, 3, 4, 0.5)";
       compare(ctx.fillStyle, '#ffaa00');
       ctx.fillStyle = "hsl(2, 3, 4, 0.8)";
       compare(ctx.fillStyle, '#ffaa00');
       ctx.fillStyle = "hsl(2, 3, 4";
       compare(ctx.fillStyle, '#ffaa00');
   }
   function test_saverestore(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       var old = ctx.fillStyle;
       ctx.save();
       ctx.fillStyle = "#ffaaff";
       ctx.restore();
       compare(ctx.fillStyle, old);

       ctx.fillStyle = "#ffcc88";
       old = ctx.fillStyle;
       ctx.save();
       compare(ctx.fillStyle, old);
       ctx.restore();
   }
   function test_namedColor(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       ctx.fillStyle = "red";
       ctx.fillRect(0,0,1,1);
       comparePixel(ctx,0,0,255,0,0,255);

       ctx.fillStyle = "black";
       ctx.fillRect(0,0,1,1);
       comparePixel(ctx,0,0,0,0,0,255);

       ctx.fillStyle = "white";
       ctx.fillRect(0,0,1,1);
       comparePixel(ctx,0,0,255,255,255,255);
   }
   function test_rgba(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       ctx.fillStyle = "rgb(-100, 300, 255)";
       compare(ctx.fillStyle, "#00ffff");
       ctx.fillStyle = "rgba(-100, 300, 255, 0.0)";
       compare(ctx.fillStyle, "rgba(0, 255, 255, 0.0)");
       ctx.fillStyle = "rgb(-10%, 110%, 50%)";
       compare(ctx.fillStyle, "#00ff80");

       ctx.clearRect(0, 0, 1, 1);
       ctx.fillStyle = 'rgba(0%, 100%, 0%, 0.499)';
       ctx.fillRect(0, 0, 1, 1);
       comparePixel(ctx, 0,0, 0,255,0,127);
   }

   function test_hsla(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       ctx.fillStyle = "hsla(120, 100%, 50%, 0.499)";
       ctx.fillRect(0, 0, 1, 1);
       comparePixel(ctx,0,0,0,255,0,127);
   }
}
