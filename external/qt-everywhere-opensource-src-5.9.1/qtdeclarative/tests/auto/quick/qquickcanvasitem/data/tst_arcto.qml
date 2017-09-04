import QtQuick 2.0

CanvasTestCase {
    id:testCase
    name: "arcTo"
    function init_data() { return testData("2d"); }

   function test_coincide(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);
       ctx.lineWidth = 50;

       ctx.strokeStyle = '#0f0';
       ctx.beginPath();
       ctx.moveTo(0, 25);
       ctx.arcTo(0, 25, 50, 1000, 1);
       ctx.lineTo(100, 25);
       ctx.stroke();

       ctx.strokeStyle = '#f00';
       ctx.beginPath();
       ctx.moveTo(50, 25);
       ctx.arcTo(50, 25, 100, 25, 1);
       ctx.stroke();

       comparePixel(ctx, 50,1, 0,255,0,255);
       comparePixel(ctx, 50,25, 0,255,0,255);
       comparePixel(ctx, 50,48, 0,255,0,255);
       ctx.reset();

       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);
       ctx.lineWidth = 50;
       ctx.strokeStyle = '#0f0';
       ctx.beginPath();
       ctx.moveTo(0, 25);
       ctx.arcTo(100, 25, 100, 25, 1);
       ctx.stroke();

       comparePixel(ctx, 50,25, 0,255,0,255);
   }
   function test_collinear(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();

       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);
       ctx.lineWidth = 50;

       ctx.strokeStyle = '#0f0';
       ctx.beginPath();
       ctx.moveTo(0, 25);
       ctx.arcTo(100, 25, 200, 25, 1);
       ctx.stroke();

       ctx.strokeStyle = '#f00';
       ctx.beginPath();
       ctx.moveTo(-100, 25);
       ctx.arcTo(0, 25, 100, 25, 1);
       ctx.stroke();

       comparePixel(ctx, 50,25, 0,255,0,255);
       ctx.reset();

       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);
       ctx.lineWidth = 50;

       ctx.strokeStyle = '#0f0';
       ctx.beginPath();
       ctx.moveTo(0, 25);
       ctx.arcTo(100, 25, 10, 25, 1);
       ctx.stroke();

       ctx.strokeStyle = '#f00';
       ctx.beginPath();
       ctx.moveTo(100, 25);
       ctx.arcTo(200, 25, 110, 25, 1);
       ctx.stroke();
       comparePixel(ctx, 50,25, 0,255,0,255);
       ctx.reset();

       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);
       ctx.lineWidth = 50;

       ctx.strokeStyle = '#0f0';
       ctx.beginPath();
       ctx.moveTo(0, 25);
       ctx.arcTo(100, 25, -100, 25, 1);
       ctx.stroke();

       ctx.strokeStyle = '#f00';
       ctx.beginPath();
       ctx.moveTo(100, 25);
       ctx.arcTo(200, 25, 0, 25, 1);
       ctx.stroke();

       ctx.beginPath();
       ctx.moveTo(-100, 25);
       ctx.arcTo(0, 25, -200, 25, 1);
       ctx.stroke();

       comparePixel(ctx, 50,25, 0,255,0,255);
   }
   function test_subpath(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();

       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);
       ctx.lineWidth = 50;
       ctx.strokeStyle = '#f00';
       ctx.beginPath();
       ctx.arcTo(100, 50, 200, 50, 0.1);
       ctx.stroke();
       comparePixel(ctx, 50,25, 0,255,0,255);
       ctx.reset();

       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);
       ctx.lineWidth = 50;
       ctx.strokeStyle = '#0f0';
       ctx.beginPath();
       ctx.arcTo(0, 25, 50, 250, 0.1);
       ctx.lineTo(100, 25);
       ctx.stroke();
       comparePixel(ctx, 50,25, 0,255,0,255);
   }

   function test_negative(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();

       try { var err = false;
         ctx.arcTo(0, 0, 0, 0, -1);
       } catch (e) {
           if (e.code != DOMException.INDEX_SIZE_ERR)
               fail("expectes INDEX_SIZE_ERR, got: "+e.message);
           err = true;
       }
       finally {
           verify(err, "should throw INDEX_SIZE_ERR: ctx.arcTo(0, 0, 0, 0, -1)");
       }
   }

   function test_nonfinite(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();

       ctx.moveTo(0, 0);
       ctx.lineTo(100, 0);
       ctx.arcTo(Infinity, 50, 0, 50, 0);
       ctx.arcTo(-Infinity, 50, 0, 50, 0);
       ctx.arcTo(NaN, 50, 0, 50, 0);
       ctx.arcTo(0, Infinity, 0, 50, 0);
       ctx.arcTo(0, -Infinity, 0, 50, 0);
       ctx.arcTo(0, NaN, 0, 50, 0);
       ctx.arcTo(0, 50, Infinity, 50, 0);
       ctx.arcTo(0, 50, -Infinity, 50, 0);
       ctx.arcTo(0, 50, NaN, 50, 0);
       ctx.arcTo(0, 50, 0, Infinity, 0);
       ctx.arcTo(0, 50, 0, -Infinity, 0);
       ctx.arcTo(0, 50, 0, NaN, 0);
       ctx.arcTo(0, 50, 0, 50, Infinity);
       ctx.arcTo(0, 50, 0, 50, -Infinity);
       ctx.arcTo(0, 50, 0, 50, NaN);
       ctx.arcTo(Infinity, Infinity, 0, 50, 0);
       ctx.arcTo(Infinity, Infinity, Infinity, 50, 0);
       ctx.arcTo(Infinity, Infinity, Infinity, Infinity, 0);
       ctx.arcTo(Infinity, Infinity, Infinity, Infinity, Infinity);
       ctx.arcTo(Infinity, Infinity, Infinity, 50, Infinity);
       ctx.arcTo(Infinity, Infinity, 0, Infinity, 0);
       ctx.arcTo(Infinity, Infinity, 0, Infinity, Infinity);
       ctx.arcTo(Infinity, Infinity, 0, 50, Infinity);
       ctx.arcTo(Infinity, 50, Infinity, 50, 0);
       ctx.arcTo(Infinity, 50, Infinity, Infinity, 0);
       ctx.arcTo(Infinity, 50, Infinity, Infinity, Infinity);
       ctx.arcTo(Infinity, 50, Infinity, 50, Infinity);
       ctx.arcTo(Infinity, 50, 0, Infinity, 0);
       ctx.arcTo(Infinity, 50, 0, Infinity, Infinity);
       ctx.arcTo(Infinity, 50, 0, 50, Infinity);
       ctx.arcTo(0, Infinity, Infinity, 50, 0);
       ctx.arcTo(0, Infinity, Infinity, Infinity, 0);
       ctx.arcTo(0, Infinity, Infinity, Infinity, Infinity);
       ctx.arcTo(0, Infinity, Infinity, 50, Infinity);
       ctx.arcTo(0, Infinity, 0, Infinity, 0);
       ctx.arcTo(0, Infinity, 0, Infinity, Infinity);
       ctx.arcTo(0, Infinity, 0, 50, Infinity);
       ctx.arcTo(0, 50, Infinity, Infinity, 0);
       ctx.arcTo(0, 50, Infinity, Infinity, Infinity);
       ctx.arcTo(0, 50, Infinity, 50, Infinity);
       ctx.arcTo(0, 50, 0, Infinity, Infinity);
       ctx.lineTo(100, 50);
       ctx.lineTo(0, 50);
       ctx.fillStyle = '#0f0';
       ctx.fill();
       comparePixel(ctx,  50,25, 0,255,0,255);
       comparePixel(ctx,  90,45, 0,255,0,255);

   }
   function test_scale(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();

       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       ctx.fillStyle = '#0f0';
       ctx.beginPath();
       ctx.moveTo(0, 50);
       ctx.translate(100, 0);
       ctx.scale(0.1, 1);
       ctx.arcTo(50, 50, 50, 0, 50);
       ctx.lineTo(-1000, 0);
       ctx.fill();

       //FIXME
       //comparePixel(ctx,  0,0, 0,255,0,255);
       //comparePixel(ctx,  50,0, 0,255,0,255);
       //comparePixel(ctx,  99,0, 0,255,0,255);
       //comparePixel(ctx,  0,25, 0,255,0,255);
       //comparePixel(ctx,  50,25, 0,255,0,255);
       //comparePixel(ctx,  99,25, 0,255,0,255);
       //comparePixel(ctx,  0,49, 0,255,0,255);
       //comparePixel(ctx,  50,49, 0,255,0,255);
       //comparePixel(ctx,  99,49, 0,255,0,255);
   }

   function test_shape(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();

       var tol = 1.5; // tolerance to avoid antialiasing artifacts

       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);

       ctx.strokeStyle = '#f00';
       ctx.lineWidth = 10;
       ctx.beginPath();
       ctx.moveTo(10, 25);
       ctx.arcTo(75, 25, 75, 60, 20);
       ctx.stroke();

       ctx.fillStyle = '#0f0';
       ctx.beginPath();
       ctx.rect(10, 20, 45, 10);
       ctx.moveTo(80, 45);
       ctx.arc(55, 45, 25+tol, 0, -Math.PI/2, true);
       ctx.arc(55, 45, 15-tol, -Math.PI/2, 0, false);
       ctx.fill();

       comparePixel(ctx,  50,25, 0,255,0,255);
       comparePixel(ctx,  55,19, 0,255,0,255);
       comparePixel(ctx,  55,20, 0,255,0,255);
       comparePixel(ctx,  55,21, 0,255,0,255);
       comparePixel(ctx,  64,22, 0,255,0,255);
       comparePixel(ctx,  65,21, 0,255,0,255);
       comparePixel(ctx,  72,28, 0,255,0,255);
       comparePixel(ctx,  73,27, 0,255,0,255);
       comparePixel(ctx,  78,36, 0,255,0,255);
       comparePixel(ctx,  79,35, 0,255,0,255);
       comparePixel(ctx,  80,44, 0,255,0,255);
       comparePixel(ctx,  80,45, 0,255,0,255);
       comparePixel(ctx,  80,46, 0,255,0,255);
       comparePixel(ctx,  65,45, 0,255,0,255);
       ctx.reset();

       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);

       ctx.fillStyle = '#f00';
       ctx.beginPath();
       ctx.rect(10, 20, 45, 10);
       ctx.moveTo(80, 45);
       ctx.arc(55, 45, 25-tol, 0, -Math.PI/2, true);
       ctx.arc(55, 45, 15+tol, -Math.PI/2, 0, false);
       ctx.fill();

       ctx.strokeStyle = '#0f0';
       ctx.lineWidth = 10;
       ctx.beginPath();
       ctx.moveTo(10, 25);
       ctx.arcTo(75, 25, 75, 60, 20);
       ctx.stroke();

       //comparePixel(ctx,  50,25, 0,255,0,255);
       comparePixel(ctx,  55,19, 0,255,0,255);
       comparePixel(ctx,  55,20, 0,255,0,255);
       if (canvas.renderTarget === Canvas.Image) {
           //FIXME:broken for Canvas.FramebufferObject
           comparePixel(ctx,  55,21, 0,255,0,255);
       }
       comparePixel(ctx,  64,22, 0,255,0,255);
       comparePixel(ctx,  65,21, 0,255,0,255);
       comparePixel(ctx,  72,28, 0,255,0,255);
       comparePixel(ctx,  73,27, 0,255,0,255);
       comparePixel(ctx,  78,36, 0,255,0,255);
       comparePixel(ctx,  79,35, 0,255,0,255);
       comparePixel(ctx,  80,44, 0,255,0,255);
       comparePixel(ctx,  80,45, 0,255,0,255);
       comparePixel(ctx,  80,46, 0,255,0,255);
       ctx.reset();


       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);
       ctx.strokeStyle = '#f00';
       ctx.lineWidth = 50;
       ctx.beginPath();
       ctx.moveTo(-100, -100);
       ctx.arcTo(-100, 25, 200, 25, 10);
       ctx.stroke();

       comparePixel(ctx,  1,1, 0,255,0,255);
       comparePixel(ctx,  1,48, 0,255,0,255);
       comparePixel(ctx,  50,25, 0,255,0,255);
       comparePixel(ctx,  98,1, 0,255,0,255);
       comparePixel(ctx,  98,48, 0,255,0,255);
       ctx.reset();

       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);
       ctx.strokeStyle = '#0f0';
       ctx.lineWidth = 50;
       ctx.beginPath();
       ctx.moveTo(0, 25);
       ctx.arcTo(200, 25, 200, 50, 10);
       ctx.stroke();

       //FIXME
       //comparePixel(ctx,  1,1, 0,255,0,255);
       //comparePixel(ctx,  1,48, 0,255,0,255);
       //comparePixel(ctx,  50,25, 0,255,0,255);
       //comparePixel(ctx,  98,1, 0,255,0,255);
       //comparePixel(ctx,  98,48, 0,255,0,255);
   }

   function test_transform(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();

       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       ctx.fillStyle = '#0f0';
       ctx.beginPath();
       ctx.moveTo(0, 50);
       ctx.translate(100, 0);
       ctx.arcTo(50, 50, 50, 0, 50);
       ctx.lineTo(-100, 0);
       ctx.fill();

       comparePixel(ctx,  0,0, 0,255,0,255);
       comparePixel(ctx,  50,0, 0,255,0,255);
       comparePixel(ctx,  99,0, 0,255,0,255);
       comparePixel(ctx,  0,25, 0,255,0,255);
       comparePixel(ctx,  50,25, 0,255,0,255);
       comparePixel(ctx,  99,25, 0,255,0,255);
       comparePixel(ctx,  0,49, 0,255,0,255);
       comparePixel(ctx,  50,49, 0,255,0,255);
       comparePixel(ctx,  99,49, 0,255,0,255);
    }
   function test_zero(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();

       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);
       ctx.lineWidth = 50;

       ctx.strokeStyle = '#0f0';
       ctx.beginPath();
       ctx.moveTo(0, 25);
       ctx.arcTo(100, 25, 100, 100, 0);
       ctx.stroke();

       ctx.strokeStyle = '#f00';
       ctx.beginPath();
       ctx.moveTo(0, -25);
       ctx.arcTo(50, -25, 50, 50, 0);
       ctx.stroke();

       comparePixel(ctx,  50,25, 0,255,0,255);
       ctx.reset();

       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);
       ctx.lineWidth = 50;

       ctx.strokeStyle = '#0f0';
       ctx.beginPath();
       ctx.moveTo(0, 25);
       ctx.arcTo(100, 25, -100, 25, 0);
       ctx.stroke();

       ctx.strokeStyle = '#f00';
       ctx.beginPath();
       ctx.moveTo(100, 25);
       ctx.arcTo(200, 25, 50, 25, 0);
       ctx.stroke();

       comparePixel(ctx,  50,25, 0,255,0,255);
   }
}
