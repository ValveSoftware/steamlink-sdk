import QtQuick 2.0

CanvasTestCase {
   id:testCase
   name: "transform"
   function init_data() { return testData("2d"); }
   function test_order(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       ctx.scale(2, 1);
       ctx.rotate(Math.PI / 2);
       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, -50, 50, 50);
       comparePixel(ctx, 75,25, 0,255,0,255);
       canvas.destroy()
  }
   function test_rotate(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       ctx.rotate(Math.PI / 2);
       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, -100, 50, 100);
       comparePixel(ctx,  50,25, 0,255,0,255);
       ctx.reset();

       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       ctx.translate(100, 10);
       ctx.rotate(Infinity);
       ctx.rotate(-Infinity);
       ctx.rotate(NaN);

       ctx.fillStyle = '#0f0';
       ctx.fillRect(-100, -10, 100, 50);

       comparePixel(ctx,  50,25, 0,255,0,255);
       ctx.reset();

       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       ctx.rotate(Math.PI); // should fail obviously if this is 3.1 degrees
       ctx.fillStyle = '#0f0';
       ctx.fillRect(-100, -50, 100, 50);
       comparePixel(ctx,  50,25, 0,255,0,255);
       ctx.reset();

       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       ctx.rotate(Math.PI * (1 + 4096)); // == pi (mod 2*pi)
       // We need about pi +/- 0.001 in order to get correct-looking results
       // 32-bit floats can store pi*4097 with precision 2^-10, so that should
       // be safe enough on reasonable implementations
       ctx.fillStyle = '#0f0';
       ctx.fillRect(-100, -50, 100, 50);
       comparePixel(ctx,  50,25, 0,255,0,255);
       comparePixel(ctx,  98,2, 0,255,0,255);
       comparePixel(ctx,  98,47, 0,255,0,255);
       ctx.reset();

       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       ctx.rotate(-Math.PI * (1 + 4096));
       ctx.fillStyle = '#0f0';
       ctx.fillRect(-100, -50, 100, 50);
       comparePixel(ctx,  50,25, 0,255,0,255);
       comparePixel(ctx,  98,2, 0,255,0,255);
       comparePixel(ctx,  98,47, 0,255,0,255);
       ctx.reset();

       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       ctx.rotate(0);
       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);
       comparePixel(ctx,  50,25, 0,255,0,255);
       canvas.destroy()
  }
   function test_scale(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();

       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       ctx.scale(2, 4);
       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 50, 12.5);
       comparePixel(ctx,  90,40, 0,255,0,255);
       ctx.reset();

       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       ctx.scale(1e5, 1e5);
       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 1, 1);
       comparePixel(ctx,  50,25, 0,255,0,255);
       ctx.reset();

       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       ctx.scale(Math.sqrt(2), Math.sqrt(2));
       ctx.scale(Math.sqrt(2), Math.sqrt(2));
       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 50, 25);
       comparePixel(ctx,  90,40, 0,255,0,255);
       ctx.reset();

       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       ctx.save();
       ctx.scale(-1, 1);
       ctx.fillStyle = '#0f0';
       ctx.fillRect(-50, 0, 50, 50);
       ctx.restore();

       ctx.save();
       ctx.scale(1, -1);
       ctx.fillStyle = '#0f0';
       ctx.fillRect(50, -50, 50, 50);
       ctx.restore();
       comparePixel(ctx,  25,25, 0,255,0,255);
       comparePixel(ctx,  75,25, 0,255,0,255);
       ctx.reset();

       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       ctx.translate(100, 10);
       ctx.scale(Infinity, 0.1);
       ctx.scale(-Infinity, 0.1);
       ctx.scale(NaN, 0.1);
       ctx.scale(0.1, Infinity);
       ctx.scale(0.1, -Infinity);
       ctx.scale(0.1, NaN);
       ctx.scale(Infinity, Infinity);

       ctx.fillStyle = '#0f0';
       ctx.fillRect(-100, -10, 100, 50);

       comparePixel(ctx,  50,25, 0,255,0,255);
       ctx.reset();

       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);

       ctx.save();
       ctx.translate(50, 0);
       ctx.scale(0, 1);
       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);
       ctx.restore();

       ctx.save();
       ctx.translate(0, 25);
       ctx.scale(1, 0);
       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);
       ctx.restore();

       // Firefox has a bug where it renders the canvas as empty and toDataURL throws an exception
       canvas.toDataURL();

       comparePixel(ctx,  50,25, 0,255,0,255);
       canvas.destroy()
   }
   function test_setTransform(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       ctx.setTransform(1/2,0, 0,1/2, 0,0);
       ctx.setTransform(2,0, 0,2, 0,0);
       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 50, 25);
       comparePixel(ctx,  75,35, 0,255,0,255);
       ctx.reset();

       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       ctx.translate(100, 10);
       ctx.setTransform(Infinity, 0, 0, 0, 0, 0);
       ctx.setTransform(-Infinity, 0, 0, 0, 0, 0);
       ctx.setTransform(NaN, 0, 0, 0, 0, 0);
       ctx.setTransform(0, Infinity, 0, 0, 0, 0);
       ctx.setTransform(0, -Infinity, 0, 0, 0, 0);
       ctx.setTransform(0, NaN, 0, 0, 0, 0);
       ctx.setTransform(0, 0, Infinity, 0, 0, 0);
       ctx.setTransform(0, 0, -Infinity, 0, 0, 0);
       ctx.setTransform(0, 0, NaN, 0, 0, 0);
       ctx.setTransform(0, 0, 0, Infinity, 0, 0);
       ctx.setTransform(0, 0, 0, -Infinity, 0, 0);
       ctx.setTransform(0, 0, 0, NaN, 0, 0);
       ctx.setTransform(0, 0, 0, 0, Infinity, 0);
       ctx.setTransform(0, 0, 0, 0, -Infinity, 0);
       ctx.setTransform(0, 0, 0, 0, NaN, 0);
       ctx.setTransform(0, 0, 0, 0, 0, Infinity);
       ctx.setTransform(0, 0, 0, 0, 0, -Infinity);
       ctx.setTransform(0, 0, 0, 0, 0, NaN);
       ctx.setTransform(Infinity, Infinity, 0, 0, 0, 0);
       ctx.setTransform(Infinity, Infinity, Infinity, 0, 0, 0);
       ctx.setTransform(Infinity, Infinity, Infinity, Infinity, 0, 0);
       ctx.setTransform(Infinity, Infinity, Infinity, Infinity, Infinity, 0);
       ctx.setTransform(Infinity, Infinity, Infinity, Infinity, Infinity, Infinity);
       ctx.setTransform(Infinity, Infinity, Infinity, Infinity, 0, Infinity);
       ctx.setTransform(Infinity, Infinity, Infinity, 0, Infinity, 0);
       ctx.setTransform(Infinity, Infinity, Infinity, 0, Infinity, Infinity);
       ctx.setTransform(Infinity, Infinity, Infinity, 0, 0, Infinity);
       ctx.setTransform(Infinity, Infinity, 0, Infinity, 0, 0);
       ctx.setTransform(Infinity, Infinity, 0, Infinity, Infinity, 0);
       ctx.setTransform(Infinity, Infinity, 0, Infinity, Infinity, Infinity);
       ctx.setTransform(Infinity, Infinity, 0, Infinity, 0, Infinity);
       ctx.setTransform(Infinity, Infinity, 0, 0, Infinity, 0);
       ctx.setTransform(Infinity, Infinity, 0, 0, Infinity, Infinity);
       ctx.setTransform(Infinity, Infinity, 0, 0, 0, Infinity);
       ctx.setTransform(Infinity, 0, Infinity, 0, 0, 0);
       ctx.setTransform(Infinity, 0, Infinity, Infinity, 0, 0);
       ctx.setTransform(Infinity, 0, Infinity, Infinity, Infinity, 0);
       ctx.setTransform(Infinity, 0, Infinity, Infinity, Infinity, Infinity);
       ctx.setTransform(Infinity, 0, Infinity, Infinity, 0, Infinity);
       ctx.setTransform(Infinity, 0, Infinity, 0, Infinity, 0);
       ctx.setTransform(Infinity, 0, Infinity, 0, Infinity, Infinity);
       ctx.setTransform(Infinity, 0, Infinity, 0, 0, Infinity);
       ctx.setTransform(Infinity, 0, 0, Infinity, 0, 0);
       ctx.setTransform(Infinity, 0, 0, Infinity, Infinity, 0);
       ctx.setTransform(Infinity, 0, 0, Infinity, Infinity, Infinity);
       ctx.setTransform(Infinity, 0, 0, Infinity, 0, Infinity);
       ctx.setTransform(Infinity, 0, 0, 0, Infinity, 0);
       ctx.setTransform(Infinity, 0, 0, 0, Infinity, Infinity);
       ctx.setTransform(Infinity, 0, 0, 0, 0, Infinity);
       ctx.setTransform(0, Infinity, Infinity, 0, 0, 0);
       ctx.setTransform(0, Infinity, Infinity, Infinity, 0, 0);
       ctx.setTransform(0, Infinity, Infinity, Infinity, Infinity, 0);
       ctx.setTransform(0, Infinity, Infinity, Infinity, Infinity, Infinity);
       ctx.setTransform(0, Infinity, Infinity, Infinity, 0, Infinity);
       ctx.setTransform(0, Infinity, Infinity, 0, Infinity, 0);
       ctx.setTransform(0, Infinity, Infinity, 0, Infinity, Infinity);
       ctx.setTransform(0, Infinity, Infinity, 0, 0, Infinity);
       ctx.setTransform(0, Infinity, 0, Infinity, 0, 0);
       ctx.setTransform(0, Infinity, 0, Infinity, Infinity, 0);
       ctx.setTransform(0, Infinity, 0, Infinity, Infinity, Infinity);
       ctx.setTransform(0, Infinity, 0, Infinity, 0, Infinity);
       ctx.setTransform(0, Infinity, 0, 0, Infinity, 0);
       ctx.setTransform(0, Infinity, 0, 0, Infinity, Infinity);
       ctx.setTransform(0, Infinity, 0, 0, 0, Infinity);
       ctx.setTransform(0, 0, Infinity, Infinity, 0, 0);
       ctx.setTransform(0, 0, Infinity, Infinity, Infinity, 0);
       ctx.setTransform(0, 0, Infinity, Infinity, Infinity, Infinity);
       ctx.setTransform(0, 0, Infinity, Infinity, 0, Infinity);
       ctx.setTransform(0, 0, Infinity, 0, Infinity, 0);
       ctx.setTransform(0, 0, Infinity, 0, Infinity, Infinity);
       ctx.setTransform(0, 0, Infinity, 0, 0, Infinity);
       ctx.setTransform(0, 0, 0, Infinity, Infinity, 0);
       ctx.setTransform(0, 0, 0, Infinity, Infinity, Infinity);
       ctx.setTransform(0, 0, 0, Infinity, 0, Infinity);
       ctx.setTransform(0, 0, 0, 0, Infinity, Infinity);

       ctx.fillStyle = '#0f0';
       ctx.fillRect(-100, -10, 100, 50);

       comparePixel(ctx,  50,25, 0,255,0,255);
       ctx.reset();

       // Create green with a red square ring inside it
       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);
       ctx.fillStyle = '#f00';
       ctx.fillRect(20, 10, 60, 30);
       ctx.fillStyle = '#0f0';
       ctx.fillRect(40, 20, 20, 10);

       // Draw a skewed shape to fill that gap, to make sure it is aligned correctly
       ctx.setTransform(1,4, 2,3, 5,6);
       // Post-transform coordinates:
       //   [[20,10],[80,10],[80,40],[20,40],[20,10],[40,20],[40,30],[60,30],[60,20],[40,20],[20,10]];
       // Hence pre-transform coordinates:
       var pts=[[-7.4,11.2],[-43.4,59.2],[-31.4,53.2],[4.6,5.2],[-7.4,11.2],
                [-15.4,25.2],[-11.4,23.2],[-23.4,39.2],[-27.4,41.2],[-15.4,25.2],
                [-7.4,11.2]];
       ctx.beginPath();
       ctx.moveTo(pts[0][0], pts[0][1]);
       for (var i = 0; i < pts.length; ++i)
           ctx.lineTo(pts[i][0], pts[i][1]);
       ctx.fill();
       /*
        //FIXME:
       comparePixel(ctx,  21,11, 0,255,0,255);
       comparePixel(ctx,  79,11, 0,255,0,255);
       comparePixel(ctx,  21,39, 0,255,0,255);
       comparePixel(ctx,  79,39, 0,255,0,255);
       comparePixel(ctx,  39,19, 0,255,0,255);
       comparePixel(ctx,  61,19, 0,255,0,255);
       comparePixel(ctx,  39,31, 0,255,0,255);
       comparePixel(ctx,  61,31, 0,255,0,255);
       */
       ctx.reset();

       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       ctx.transform(1,0, 0,1, 0,0);
       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);
       comparePixel(ctx,  50,25, 0,255,0,255);
       canvas.destroy()
  }
   function test_transform(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       ctx.transform(1,2, 3,4, 5,6);
       ctx.transform(-2,1, 3/2,-1/2, 1,-2);
       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);
       comparePixel(ctx,  50,25, 0,255,0,255);
       ctx.reset();

       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       ctx.translate(100, 10);
       ctx.transform(Infinity, 0, 0, 0, 0, 0);
       ctx.transform(-Infinity, 0, 0, 0, 0, 0);
       ctx.transform(NaN, 0, 0, 0, 0, 0);
       ctx.transform(0, Infinity, 0, 0, 0, 0);
       ctx.transform(0, -Infinity, 0, 0, 0, 0);
       ctx.transform(0, NaN, 0, 0, 0, 0);
       ctx.transform(0, 0, Infinity, 0, 0, 0);
       ctx.transform(0, 0, -Infinity, 0, 0, 0);
       ctx.transform(0, 0, NaN, 0, 0, 0);
       ctx.transform(0, 0, 0, Infinity, 0, 0);
       ctx.transform(0, 0, 0, -Infinity, 0, 0);
       ctx.transform(0, 0, 0, NaN, 0, 0);
       ctx.transform(0, 0, 0, 0, Infinity, 0);
       ctx.transform(0, 0, 0, 0, -Infinity, 0);
       ctx.transform(0, 0, 0, 0, NaN, 0);
       ctx.transform(0, 0, 0, 0, 0, Infinity);
       ctx.transform(0, 0, 0, 0, 0, -Infinity);
       ctx.transform(0, 0, 0, 0, 0, NaN);
       ctx.transform(Infinity, Infinity, 0, 0, 0, 0);
       ctx.transform(Infinity, Infinity, Infinity, 0, 0, 0);
       ctx.transform(Infinity, Infinity, Infinity, Infinity, 0, 0);
       ctx.transform(Infinity, Infinity, Infinity, Infinity, Infinity, 0);
       ctx.transform(Infinity, Infinity, Infinity, Infinity, Infinity, Infinity);
       ctx.transform(Infinity, Infinity, Infinity, Infinity, 0, Infinity);
       ctx.transform(Infinity, Infinity, Infinity, 0, Infinity, 0);
       ctx.transform(Infinity, Infinity, Infinity, 0, Infinity, Infinity);
       ctx.transform(Infinity, Infinity, Infinity, 0, 0, Infinity);
       ctx.transform(Infinity, Infinity, 0, Infinity, 0, 0);
       ctx.transform(Infinity, Infinity, 0, Infinity, Infinity, 0);
       ctx.transform(Infinity, Infinity, 0, Infinity, Infinity, Infinity);
       ctx.transform(Infinity, Infinity, 0, Infinity, 0, Infinity);
       ctx.transform(Infinity, Infinity, 0, 0, Infinity, 0);
       ctx.transform(Infinity, Infinity, 0, 0, Infinity, Infinity);
       ctx.transform(Infinity, Infinity, 0, 0, 0, Infinity);
       ctx.transform(Infinity, 0, Infinity, 0, 0, 0);
       ctx.transform(Infinity, 0, Infinity, Infinity, 0, 0);
       ctx.transform(Infinity, 0, Infinity, Infinity, Infinity, 0);
       ctx.transform(Infinity, 0, Infinity, Infinity, Infinity, Infinity);
       ctx.transform(Infinity, 0, Infinity, Infinity, 0, Infinity);
       ctx.transform(Infinity, 0, Infinity, 0, Infinity, 0);
       ctx.transform(Infinity, 0, Infinity, 0, Infinity, 0);
       ctx.transform(Infinity, 0, Infinity, 0, Infinity, Infinity);
       ctx.transform(Infinity, 0, Infinity, 0, 0, Infinity);
       ctx.transform(Infinity, 0, 0, Infinity, 0, 0);
       ctx.transform(Infinity, 0, 0, Infinity, Infinity, 0);
       ctx.transform(Infinity, 0, 0, Infinity, Infinity, Infinity);
       ctx.transform(Infinity, 0, 0, Infinity, 0, Infinity);
       ctx.transform(Infinity, 0, 0, 0, Infinity, 0);
       ctx.transform(Infinity, 0, 0, 0, Infinity, Infinity);
       ctx.transform(Infinity, 0, 0, 0, 0, Infinity);
       ctx.transform(0, Infinity, Infinity, 0, 0, 0);
       ctx.transform(0, Infinity, Infinity, Infinity, 0, 0);
       ctx.transform(0, Infinity, Infinity, Infinity, Infinity, 0);
       ctx.transform(0, Infinity, Infinity, Infinity, Infinity, Infinity);
       ctx.transform(0, Infinity, Infinity, Infinity, 0, Infinity);
       ctx.transform(0, Infinity, Infinity, 0, Infinity, 0);
       ctx.transform(0, Infinity, Infinity, 0, Infinity, Infinity);
       ctx.transform(0, Infinity, Infinity, 0, 0, Infinity);
       ctx.transform(0, Infinity, 0, Infinity, 0, 0);
       ctx.transform(0, Infinity, 0, Infinity, Infinity, 0);
       ctx.transform(0, Infinity, 0, Infinity, Infinity, Infinity);
       ctx.transform(0, Infinity, 0, Infinity, 0, Infinity);
       ctx.transform(0, Infinity, 0, 0, Infinity, 0);
       ctx.transform(0, Infinity, 0, 0, Infinity, Infinity);
       ctx.transform(0, Infinity, 0, 0, 0, Infinity);
       ctx.transform(0, 0, Infinity, Infinity, 0, 0);
       ctx.transform(0, 0, Infinity, Infinity, Infinity, 0);
       ctx.transform(0, 0, Infinity, Infinity, Infinity, Infinity);
       ctx.transform(0, 0, Infinity, Infinity, 0, Infinity);
       ctx.transform(0, 0, Infinity, 0, Infinity, 0);
       ctx.transform(0, 0, Infinity, 0, Infinity, Infinity);
       ctx.transform(0, 0, Infinity, 0, 0, Infinity);
       ctx.transform(0, 0, 0, Infinity, Infinity, 0);
       ctx.transform(0, 0, 0, Infinity, Infinity, Infinity);
       ctx.transform(0, 0, 0, Infinity, 0, Infinity);
       ctx.transform(0, 0, 0, 0, Infinity, Infinity);

       ctx.fillStyle = '#0f0';
       ctx.fillRect(-100, -10, 100, 50);

       comparePixel(ctx,  50,25, 0,255,0,255);
       ctx.reset();

       // Create green with a red square ring inside it
       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);
       ctx.fillStyle = '#f00';
       ctx.fillRect(20, 10, 60, 30);
       ctx.fillStyle = '#0f0';
       ctx.fillRect(40, 20, 20, 10);

       // Draw a skewed shape to fill that gap, to make sure it is aligned correctly
       ctx.transform(1,4, 2,3, 5,6);
       // Post-transform coordinates:
       //   [[20,10],[80,10],[80,40],[20,40],[20,10],[40,20],[40,30],[60,30],[60,20],[40,20],[20,10]];
       // Hence pre-transform coordinates:
       var pts=[[-7.4,11.2],[-43.4,59.2],[-31.4,53.2],[4.6,5.2],[-7.4,11.2],
                [-15.4,25.2],[-11.4,23.2],[-23.4,39.2],[-27.4,41.2],[-15.4,25.2],
                [-7.4,11.2]];
       ctx.beginPath();
       ctx.moveTo(pts[0][0], pts[0][1]);
       for (var i = 0; i < pts.length; ++i)
           ctx.lineTo(pts[i][0], pts[i][1]);
       ctx.fill();
       /*
         //FIXME:
       comparePixel(ctx,  21,11, 0,255,0,255);
       comparePixel(ctx,  79,11, 0,255,0,255);
       comparePixel(ctx,  21,39, 0,255,0,255);
       comparePixel(ctx,  79,39, 0,255,0,255);
       comparePixel(ctx,  39,19, 0,255,0,255);
       comparePixel(ctx,  61,19, 0,255,0,255);
       comparePixel(ctx,  39,31, 0,255,0,255);
       comparePixel(ctx,  61,31, 0,255,0,255);
       */
       canvas.destroy()
  }
   function test_translate(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       ctx.translate(100, 50);
       ctx.fillStyle = '#0f0';
       ctx.fillRect(-100, -50, 100, 50);
       comparePixel(ctx,  90,40, 0,255,0,255);
       ctx.reset();

       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       ctx.translate(100, 10);
       ctx.translate(Infinity, 0.1);
       ctx.translate(-Infinity, 0.1);
       ctx.translate(NaN, 0.1);
       ctx.translate(0.1, Infinity);
       ctx.translate(0.1, -Infinity);
       ctx.translate(0.1, NaN);
       ctx.translate(Infinity, Infinity);

       ctx.fillStyle = '#0f0';
       ctx.fillRect(-100, -10, 100, 50);

       comparePixel(ctx,  50,25, 0,255,0,255);
       canvas.destroy()
  }
}
