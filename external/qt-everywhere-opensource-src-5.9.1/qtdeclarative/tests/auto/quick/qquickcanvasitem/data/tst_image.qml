import QtQuick 2.0

CanvasTestCase {
   id:testCase
   name: "image"
   function init_data() { return testData("2d"); }

   function loadImages(canvas) {
       canvas.loadImage('green.png');
       canvas.loadImage('red.png');
       canvas.loadImage('rgrg-256x256.png');
       canvas.loadImage('ggrr-256x256.png');
       canvas.loadImage('broken.png');
       while (!canvas.isImageLoaded('green.png'))
          wait(200);
   }

   function test_3args(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       loadImages(canvas);
       ctx.reset();

       ctx.drawImage('green.png', 0, 0);
       ctx.drawImage('red.png', -100, 0);
       ctx.drawImage('red.png', 100, 0);
       ctx.drawImage('red.png', 0, -50);
       ctx.drawImage('red.png', 0, 50);

       comparePixel(ctx, 0,0, 0,255,0,255,2);
       comparePixel(ctx, 99,0, 0,255,0,255,2);
       comparePixel(ctx, 0,49, 0,255,0,255,2);
       comparePixel(ctx, 99,49, 0,255,0,255,2);

  }
   function test_5args(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       loadImages(canvas);

       ctx.reset();
       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);
       ctx.drawImage('green.png', 50, 0, 50, 50);
       ctx.drawImage('red.png', 0, 0, 50, 50);
       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 50, 50);

       comparePixel(ctx, 0,0, 0,255,0,255,2);
       comparePixel(ctx, 99,0, 0,255,0,255,2);
       comparePixel(ctx, 0,49, 0,255,0,255,2);
       comparePixel(ctx, 99,49, 0,255,0,255,2);

  }
   function test_9args(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       loadImages(canvas);

       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);
       ctx.drawImage('green.png', 0, 0, 100, 50, 0, 0, 100, 50);
       comparePixel(ctx, 0,0, 0,255,0,255,2);
       comparePixel(ctx, 99,0, 0,255,0,255,2);
       comparePixel(ctx, 0,49, 0,255,0,255,2);
       comparePixel(ctx, 99,49, 0,255,0,255,2);

       ctx.reset();

       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);
       ctx.drawImage('green.png', 0, 0, 100, 50, 0, 0, 100, 50);
       ctx.drawImage('red.png', 0, 0, 100, 50, -100, 0, 100, 50);
       ctx.drawImage('red.png', 0, 0, 100, 50, 100, 0, 100, 50);
       ctx.drawImage('red.png', 0, 0, 100, 50, 0, -50, 100, 50);
       ctx.drawImage('red.png', 0, 0, 100, 50, 0, 50, 100, 50);
       comparePixel(ctx, 0,0, 0,255,0,255,2);
       comparePixel(ctx, 99,0, 0,255,0,255,2);
       comparePixel(ctx, 0,49, 0,255,0,255,2);
       comparePixel(ctx, 99,49, 0,255,0,255,2);


       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);
       ctx.drawImage('green.png', 1, 1, 1, 1, 0, 0, 100, 50);
       ctx.drawImage('red.png', 0, 0, 100, 50, -50, 0, 50, 50);
       ctx.drawImage('red.png', 0, 0, 100, 50, 100, 0, 50, 50);
       ctx.drawImage('red.png', 0, 0, 100, 50, 0, -25, 100, 25);
       ctx.drawImage('red.png', 0, 0, 100, 50, 0, 50, 100, 25);
       comparePixel(ctx, 0,0, 0,255,0,255,2);
       comparePixel(ctx, 99,0, 0,255,0,255,2);
       comparePixel(ctx, 0,49, 0,255,0,255,2);
       comparePixel(ctx, 99,49, 0,255,0,255,2);

       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);
       ctx.drawImage('rgrg-256x256.png', 140, 20, 100, 50, 0, 0, 100, 50);
       comparePixel(ctx, 0,0, 0,255,0,255,2);
       comparePixel(ctx, 99,0, 0,255,0,255,2);
       comparePixel(ctx, 0,49, 0,255,0,255,2);
       comparePixel(ctx, 99,49, 0,255,0,255,2);

       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);
       ctx.drawImage('rgrg-256x256.png', 0, 0, 256, 256, 0, 0, 100, 50);
       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 51, 26);
       ctx.fillRect(49, 24, 51, 26);
       comparePixel(ctx, 0,0, 0,255,0,255,2);
       comparePixel(ctx, 99,0, 0,255,0,255,2);
       comparePixel(ctx, 0,49, 0,255,0,255,2);
       comparePixel(ctx, 99,49, 0,255,0,255,2);
       comparePixel(ctx, 20,20, 0,255,0,255,2);
       comparePixel(ctx, 80,20, 0,255,0,255,2);
       comparePixel(ctx, 20,30, 0,255,0,255,2);
       comparePixel(ctx, 80,30, 0,255,0,255,2);

   }
   function test_animated(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       loadImages(canvas);

       //should animated image be supported at all?
  }
   function test_clip(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       loadImages(canvas);

       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);
       ctx.rect(-10, -10, 1, 1);
       ctx.clip();
       ctx.drawImage('red.png', 0, 0);
       comparePixel(ctx, 50,25, 0,255,0,255,2);


  }
   function test_self(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       loadImages(canvas);

       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 50, 50);
       ctx.fillStyle = '#f00';
       ctx.fillRect(50, 0, 50, 50);
       ctx.drawImage(canvas, 50, 0);

       comparePixel(ctx, 0,0, 0,255,0,255,2);
       comparePixel(ctx, 99,0, 0,255,0,255,2);
       comparePixel(ctx, 0,49, 0,255,0,255,2);
       comparePixel(ctx, 99,49, 0,255,0,255,2);

       ctx.reset();
       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 1, 100, 49);
       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 1);
       ctx.drawImage(canvas, 0, 1);
       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 2);

       comparePixel(ctx, 0,0, 0,255,0,255,2);
       comparePixel(ctx, 99,0, 0,255,0,255,2);
       comparePixel(ctx, 0,49, 0,255,0,255,2);
       comparePixel(ctx, 99,49, 0,255,0,255,2);
   }

   function test_outsidesource(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       loadImages(canvas);


       ctx.drawImage('green.png', 10.5, 10.5, 89.5, 39.5, 0, 0, 100, 50);
       ctx.drawImage('green.png', 5.5, 5.5, -5.5, -5.5, 0, 0, 100, 50);
       ctx.drawImage('green.png', 100, 50, -5, -5, 0, 0, 100, 50);
       try { var err = false;
         ctx.drawImage('red.png', -0.001, 0, 100, 50, 0, 0, 100, 50);
       } catch (e) { if (e.code != DOMException.INDEX_SIZE_ERR) fail("Failed assertion: expected exception of type INDEX_SIZE_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type INDEX_SIZE_ERR: ctx.drawImage('red.png', -0.001, 0, 100, 50, 0, 0, 100, 50)"); }
       try { var err = false;
         ctx.drawImage('red.png', 0, -0.001, 100, 50, 0, 0, 100, 50);
       } catch (e) { if (e.code != DOMException.INDEX_SIZE_ERR) fail("Failed assertion: expected exception of type INDEX_SIZE_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type INDEX_SIZE_ERR: ctx.drawImage('red.png', 0, -0.001, 100, 50, 0, 0, 100, 50)"); }
       try { var err = false;
         ctx.drawImage('red.png', 0, 0, 100.001, 50, 0, 0, 100, 50);
       } catch (e) { if (e.code != DOMException.INDEX_SIZE_ERR) fail("Failed assertion: expected exception of type INDEX_SIZE_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type INDEX_SIZE_ERR: ctx.drawImage('red.png', 0, 0, 100.001, 50, 0, 0, 100, 50)"); }
       try { var err = false;
         ctx.drawImage('red.png', 0, 0, 100, 50.001, 0, 0, 100, 50);
       } catch (e) { if (e.code != DOMException.INDEX_SIZE_ERR) fail("Failed assertion: expected exception of type INDEX_SIZE_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type INDEX_SIZE_ERR: ctx.drawImage('red.png', 0, 0, 100, 50.001, 0, 0, 100, 50)"); }
       try { var err = false;
         ctx.drawImage('red.png', 50, 0, 50.001, 50, 0, 0, 100, 50);
       } catch (e) { if (e.code != DOMException.INDEX_SIZE_ERR) fail("Failed assertion: expected exception of type INDEX_SIZE_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type INDEX_SIZE_ERR: ctx.drawImage('red.png', 50, 0, 50.001, 50, 0, 0, 100, 50)"); }
       try { var err = false;
         ctx.drawImage('red.png', 0, 0, -5, 5, 0, 0, 100, 50);
       } catch (e) { if (e.code != DOMException.INDEX_SIZE_ERR) fail("Failed assertion: expected exception of type INDEX_SIZE_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type INDEX_SIZE_ERR: ctx.drawImage('red.png', 0, 0, -5, 5, 0, 0, 100, 50)"); }
       try { var err = false;
         ctx.drawImage('red.png', 0, 0, 5, -5, 0, 0, 100, 50);
       } catch (e) { if (e.code != DOMException.INDEX_SIZE_ERR) fail("Failed assertion: expected exception of type INDEX_SIZE_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type INDEX_SIZE_ERR: ctx.drawImage('red.png', 0, 0, 5, -5, 0, 0, 100, 50)"); }
//           try { var err = false;
//             ctx.drawImage('red.png', 110, 60, -20, -20, 0, 0, 100, 50);
//           } catch (e) { if (e.code != DOMException.INDEX_SIZE_ERR) fail("Failed assertion: expected exception of type INDEX_SIZE_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type INDEX_SIZE_ERR: ctx.drawImage('red.png', 110, 60, -20, -20, 0, 0, 100, 50)"); }
//           comparePixel(ctx, 50,25, 0,255,0,255,2);

   }

   function test_null(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       loadImages(canvas);

       try { var err = false;
         ctx.drawImage(null, 0, 0);
       } catch (e) { if (e.code != DOMException.TYPE_MISMATCH_ERR) fail("Failed assertion: expected exception of type TYPE_MISMATCH_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type TYPE_MISMATCH_ERR: ctx.drawImage(null, 0, 0)"); }

   }

   property url green: 'green.png'

   function test_url(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');

       canvas.loadImage(testCase.green);
       ctx.drawImage(testCase.green, 0, 0);
       comparePixel(ctx, 0,0, 0,255,0,255,2);
   }

   function test_composite(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       loadImages(canvas);

       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);
       ctx.globalCompositeOperation = 'destination-over';
       ctx.drawImage('red.png', 0, 0);
       comparePixel(ctx, 50,25, 0,255,0,255,2);

  }
   function test_path(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       loadImages(canvas);

  }
   function test_transform(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       loadImages(canvas);

       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);
       ctx.translate(100, 0);
       ctx.drawImage('red.png', 0, 0);
       comparePixel(ctx, 50,25, 0,255,0,255,2);

  }

   function test_imageitem(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       loadImages(canvas);

       //TODO
   }

   function test_imageData(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       loadImages(canvas);

       //TODO
   }

   function test_wrongtype(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       loadImages(canvas);


       try { var err = false;
         ctx.drawImage(undefined, 0, 0);
       } catch (e) { if (e.code != DOMException.TYPE_MISMATCH_ERR) fail("Failed assertion: expected exception of type TYPE_MISMATCH_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type TYPE_MISMATCH_ERR: ctx.drawImage(undefined, 0, 0)"); }
       try { var err = false;
         ctx.drawImage(0, 0, 0);
       } catch (e) { if (e.code != DOMException.TYPE_MISMATCH_ERR) fail("Failed assertion: expected exception of type TYPE_MISMATCH_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type TYPE_MISMATCH_ERR: ctx.drawImage(0, 0, 0)"); }
       try { var err = false;
         ctx.drawImage("", 0, 0);
       } catch (e) { if (e.code != DOMException.TYPE_MISMATCH_ERR) fail("Failed assertion: expected exception of type TYPE_MISMATCH_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type TYPE_MISMATCH_ERR: ctx.drawImage(\"\", 0, 0)"); }
   }

   function test_nonfinite(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       loadImages(canvas);

       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);
       var red = 'red.png';
       ctx.drawImage(red, Infinity, 0);
       ctx.drawImage(red, -Infinity, 0);
       ctx.drawImage(red, NaN, 0);
       ctx.drawImage(red, 0, Infinity);
       ctx.drawImage(red, 0, -Infinity);
       ctx.drawImage(red, 0, NaN);
       ctx.drawImage(red, Infinity, Infinity);
       ctx.drawImage(red, Infinity, 0, 100, 50);
       ctx.drawImage(red, -Infinity, 0, 100, 50);
       ctx.drawImage(red, NaN, 0, 100, 50);
       ctx.drawImage(red, 0, Infinity, 100, 50);
       ctx.drawImage(red, 0, -Infinity, 100, 50);
       ctx.drawImage(red, 0, NaN, 100, 50);
       ctx.drawImage(red, 0, 0, Infinity, 50);
       ctx.drawImage(red, 0, 0, -Infinity, 50);
       ctx.drawImage(red, 0, 0, NaN, 50);
       ctx.drawImage(red, 0, 0, 100, Infinity);
       ctx.drawImage(red, 0, 0, 100, -Infinity);
       ctx.drawImage(red, 0, 0, 100, NaN);
       ctx.drawImage(red, Infinity, Infinity, 100, 50);
       ctx.drawImage(red, Infinity, Infinity, Infinity, 50);
       ctx.drawImage(red, Infinity, Infinity, Infinity, Infinity);
       ctx.drawImage(red, Infinity, Infinity, 100, Infinity);
       ctx.drawImage(red, Infinity, 0, Infinity, 50);
       ctx.drawImage(red, Infinity, 0, Infinity, Infinity);
       ctx.drawImage(red, Infinity, 0, 100, Infinity);
       ctx.drawImage(red, 0, Infinity, Infinity, 50);
       ctx.drawImage(red, 0, Infinity, Infinity, Infinity);
       ctx.drawImage(red, 0, Infinity, 100, Infinity);
       ctx.drawImage(red, 0, 0, Infinity, Infinity);
       ctx.drawImage(red, Infinity, 0, 100, 50, 0, 0, 100, 50);
       ctx.drawImage(red, -Infinity, 0, 100, 50, 0, 0, 100, 50);
       ctx.drawImage(red, NaN, 0, 100, 50, 0, 0, 100, 50);
       ctx.drawImage(red, 0, Infinity, 100, 50, 0, 0, 100, 50);
       ctx.drawImage(red, 0, -Infinity, 100, 50, 0, 0, 100, 50);
       ctx.drawImage(red, 0, NaN, 100, 50, 0, 0, 100, 50);
       ctx.drawImage(red, 0, 0, Infinity, 50, 0, 0, 100, 50);
       ctx.drawImage(red, 0, 0, -Infinity, 50, 0, 0, 100, 50);
       ctx.drawImage(red, 0, 0, NaN, 50, 0, 0, 100, 50);
       ctx.drawImage(red, 0, 0, 100, Infinity, 0, 0, 100, 50);
       ctx.drawImage(red, 0, 0, 100, -Infinity, 0, 0, 100, 50);
       ctx.drawImage(red, 0, 0, 100, NaN, 0, 0, 100, 50);
       ctx.drawImage(red, 0, 0, 100, 50, Infinity, 0, 100, 50);
       ctx.drawImage(red, 0, 0, 100, 50, -Infinity, 0, 100, 50);
       ctx.drawImage(red, 0, 0, 100, 50, NaN, 0, 100, 50);
       ctx.drawImage(red, 0, 0, 100, 50, 0, Infinity, 100, 50);
       ctx.drawImage(red, 0, 0, 100, 50, 0, -Infinity, 100, 50);
       ctx.drawImage(red, 0, 0, 100, 50, 0, NaN, 100, 50);
       ctx.drawImage(red, 0, 0, 100, 50, 0, 0, Infinity, 50);
       ctx.drawImage(red, 0, 0, 100, 50, 0, 0, -Infinity, 50);
       ctx.drawImage(red, 0, 0, 100, 50, 0, 0, NaN, 50);
       ctx.drawImage(red, 0, 0, 100, 50, 0, 0, 100, Infinity);
       ctx.drawImage(red, 0, 0, 100, 50, 0, 0, 100, -Infinity);
       ctx.drawImage(red, 0, 0, 100, 50, 0, 0, 100, NaN);
       ctx.drawImage(red, Infinity, Infinity, 100, 50, 0, 0, 100, 50);
       ctx.drawImage(red, Infinity, Infinity, Infinity, 50, 0, 0, 100, 50);
       ctx.drawImage(red, Infinity, Infinity, Infinity, Infinity, 0, 0, 100, 50);
       ctx.drawImage(red, Infinity, Infinity, Infinity, Infinity, Infinity, 0, 100, 50);
       ctx.drawImage(red, Infinity, Infinity, Infinity, Infinity, Infinity, Infinity, 100, 50);
       ctx.drawImage(red, Infinity, Infinity, Infinity, Infinity, Infinity, Infinity, Infinity, 50);
       ctx.drawImage(red, Infinity, Infinity, Infinity, Infinity, Infinity, Infinity, Infinity, Infinity);
       ctx.drawImage(red, Infinity, Infinity, Infinity, Infinity, Infinity, Infinity, 100, Infinity);
       ctx.drawImage(red, Infinity, Infinity, Infinity, Infinity, Infinity, 0, Infinity, 50);
       ctx.drawImage(red, Infinity, Infinity, Infinity, Infinity, Infinity, 0, Infinity, Infinity);
       ctx.drawImage(red, Infinity, Infinity, Infinity, Infinity, Infinity, 0, 100, Infinity);
       ctx.drawImage(red, Infinity, Infinity, Infinity, Infinity, 0, Infinity, 100, 50);
       ctx.drawImage(red, Infinity, Infinity, Infinity, Infinity, 0, Infinity, Infinity, 50);
       ctx.drawImage(red, Infinity, Infinity, Infinity, Infinity, 0, Infinity, Infinity, Infinity);
       ctx.drawImage(red, Infinity, Infinity, Infinity, Infinity, 0, Infinity, 100, Infinity);
       ctx.drawImage(red, Infinity, Infinity, Infinity, Infinity, 0, 0, Infinity, 50);
       ctx.drawImage(red, Infinity, Infinity, Infinity, Infinity, 0, 0, Infinity, Infinity);
       ctx.drawImage(red, Infinity, Infinity, Infinity, Infinity, 0, 0, 100, Infinity);
       ctx.drawImage(red, Infinity, Infinity, Infinity, 50, Infinity, 0, 100, 50);
       ctx.drawImage(red, Infinity, Infinity, Infinity, 50, Infinity, Infinity, 100, 50);
       ctx.drawImage(red, Infinity, Infinity, Infinity, 50, Infinity, Infinity, Infinity, 50);
       ctx.drawImage(red, Infinity, Infinity, Infinity, 50, Infinity, Infinity, Infinity, Infinity);
       ctx.drawImage(red, Infinity, Infinity, Infinity, 50, Infinity, Infinity, 100, Infinity);
       ctx.drawImage(red, Infinity, Infinity, Infinity, 50, Infinity, 0, Infinity, 50);
       ctx.drawImage(red, Infinity, Infinity, Infinity, 50, Infinity, 0, Infinity, Infinity);
       ctx.drawImage(red, Infinity, Infinity, Infinity, 50, Infinity, 0, 100, Infinity);
       ctx.drawImage(red, Infinity, Infinity, Infinity, 50, 0, Infinity, 100, 50);
       ctx.drawImage(red, Infinity, Infinity, Infinity, 50, 0, Infinity, Infinity, 50);
       ctx.drawImage(red, Infinity, Infinity, Infinity, 50, 0, Infinity, Infinity, Infinity);
       ctx.drawImage(red, Infinity, Infinity, Infinity, 50, 0, Infinity, 100, Infinity);
       ctx.drawImage(red, Infinity, Infinity, Infinity, 50, 0, 0, Infinity, 50);
       ctx.drawImage(red, Infinity, Infinity, Infinity, 50, 0, 0, Infinity, Infinity);
       ctx.drawImage(red, Infinity, Infinity, Infinity, 50, 0, 0, 100, Infinity);
       ctx.drawImage(red, Infinity, Infinity, 100, Infinity, 0, 0, 100, 50);
       ctx.drawImage(red, Infinity, Infinity, 100, Infinity, Infinity, 0, 100, 50);
       ctx.drawImage(red, Infinity, Infinity, 100, Infinity, Infinity, Infinity, 100, 50);
       ctx.drawImage(red, Infinity, Infinity, 100, Infinity, Infinity, Infinity, Infinity, 50);
       ctx.drawImage(red, Infinity, Infinity, 100, Infinity, Infinity, Infinity, Infinity, Infinity);
       ctx.drawImage(red, Infinity, Infinity, 100, Infinity, Infinity, Infinity, 100, Infinity);
       ctx.drawImage(red, Infinity, Infinity, 100, Infinity, Infinity, 0, Infinity, 50);
       ctx.drawImage(red, Infinity, Infinity, 100, Infinity, Infinity, 0, Infinity, Infinity);
       ctx.drawImage(red, Infinity, Infinity, 100, Infinity, Infinity, 0, 100, Infinity);
       ctx.drawImage(red, Infinity, Infinity, 100, Infinity, 0, Infinity, 100, 50);
       ctx.drawImage(red, Infinity, Infinity, 100, Infinity, 0, Infinity, Infinity, 50);
       ctx.drawImage(red, Infinity, Infinity, 100, Infinity, 0, Infinity, Infinity, Infinity);
       ctx.drawImage(red, Infinity, Infinity, 100, Infinity, 0, Infinity, 100, Infinity);
       ctx.drawImage(red, Infinity, Infinity, 100, Infinity, 0, 0, Infinity, 50);
       ctx.drawImage(red, Infinity, Infinity, 100, Infinity, 0, 0, Infinity, Infinity);
       ctx.drawImage(red, Infinity, Infinity, 100, Infinity, 0, 0, 100, Infinity);
       ctx.drawImage(red, Infinity, Infinity, 100, 50, Infinity, 0, 100, 50);
       ctx.drawImage(red, Infinity, Infinity, 100, 50, Infinity, Infinity, 100, 50);
       ctx.drawImage(red, Infinity, Infinity, 100, 50, Infinity, Infinity, Infinity, 50);
       ctx.drawImage(red, Infinity, Infinity, 100, 50, Infinity, Infinity, Infinity, Infinity);
       ctx.drawImage(red, Infinity, Infinity, 100, 50, Infinity, Infinity, 100, Infinity);
       ctx.drawImage(red, Infinity, Infinity, 100, 50, Infinity, 0, Infinity, 50);
       ctx.drawImage(red, Infinity, Infinity, 100, 50, Infinity, 0, Infinity, Infinity);
       ctx.drawImage(red, Infinity, Infinity, 100, 50, Infinity, 0, 100, Infinity);
       ctx.drawImage(red, Infinity, Infinity, 100, 50, 0, Infinity, 100, 50);
       ctx.drawImage(red, Infinity, Infinity, 100, 50, 0, Infinity, Infinity, 50);
       ctx.drawImage(red, Infinity, Infinity, 100, 50, 0, Infinity, Infinity, Infinity);
       ctx.drawImage(red, Infinity, Infinity, 100, 50, 0, Infinity, 100, Infinity);
       ctx.drawImage(red, Infinity, Infinity, 100, 50, 0, 0, Infinity, 50);
       ctx.drawImage(red, Infinity, Infinity, 100, 50, 0, 0, Infinity, Infinity);
       ctx.drawImage(red, Infinity, Infinity, 100, 50, 0, 0, 100, Infinity);
       ctx.drawImage(red, Infinity, 0, Infinity, 50, 0, 0, 100, 50);
       ctx.drawImage(red, Infinity, 0, Infinity, Infinity, 0, 0, 100, 50);
       ctx.drawImage(red, Infinity, 0, Infinity, Infinity, Infinity, 0, 100, 50);
       ctx.drawImage(red, Infinity, 0, Infinity, Infinity, Infinity, Infinity, 100, 50);
       ctx.drawImage(red, Infinity, 0, Infinity, Infinity, Infinity, Infinity, Infinity, 50);
       ctx.drawImage(red, Infinity, 0, Infinity, Infinity, Infinity, Infinity, Infinity, Infinity);
       ctx.drawImage(red, Infinity, 0, Infinity, Infinity, Infinity, Infinity, 100, Infinity);
       ctx.drawImage(red, Infinity, 0, Infinity, Infinity, Infinity, 0, Infinity, 50);
       ctx.drawImage(red, Infinity, 0, Infinity, Infinity, Infinity, 0, Infinity, Infinity);
       ctx.drawImage(red, Infinity, 0, Infinity, Infinity, Infinity, 0, 100, Infinity);
       ctx.drawImage(red, Infinity, 0, Infinity, Infinity, 0, Infinity, 100, 50);
       ctx.drawImage(red, Infinity, 0, Infinity, Infinity, 0, Infinity, Infinity, 50);
       ctx.drawImage(red, Infinity, 0, Infinity, Infinity, 0, Infinity, Infinity, Infinity);
       ctx.drawImage(red, Infinity, 0, Infinity, Infinity, 0, Infinity, 100, Infinity);
       ctx.drawImage(red, Infinity, 0, Infinity, Infinity, 0, 0, Infinity, 50);
       ctx.drawImage(red, Infinity, 0, Infinity, Infinity, 0, 0, Infinity, Infinity);
       ctx.drawImage(red, Infinity, 0, Infinity, Infinity, 0, 0, 100, Infinity);
       ctx.drawImage(red, Infinity, 0, Infinity, 50, Infinity, 0, 100, 50);
       ctx.drawImage(red, Infinity, 0, Infinity, 50, Infinity, Infinity, 100, 50);
       ctx.drawImage(red, Infinity, 0, Infinity, 50, Infinity, Infinity, Infinity, 50);
       ctx.drawImage(red, Infinity, 0, Infinity, 50, Infinity, Infinity, Infinity, Infinity);
       ctx.drawImage(red, Infinity, 0, Infinity, 50, Infinity, Infinity, 100, Infinity);
       ctx.drawImage(red, Infinity, 0, Infinity, 50, Infinity, 0, Infinity, 50);
       ctx.drawImage(red, Infinity, 0, Infinity, 50, Infinity, 0, Infinity, Infinity);
       ctx.drawImage(red, Infinity, 0, Infinity, 50, Infinity, 0, 100, Infinity);
       ctx.drawImage(red, Infinity, 0, Infinity, 50, 0, Infinity, 100, 50);
       ctx.drawImage(red, Infinity, 0, Infinity, 50, 0, Infinity, Infinity, 50);
       ctx.drawImage(red, Infinity, 0, Infinity, 50, 0, Infinity, Infinity, Infinity);
       ctx.drawImage(red, Infinity, 0, Infinity, 50, 0, Infinity, 100, Infinity);
       ctx.drawImage(red, Infinity, 0, Infinity, 50, 0, 0, Infinity, 50);
       ctx.drawImage(red, Infinity, 0, Infinity, 50, 0, 0, Infinity, Infinity);
       ctx.drawImage(red, Infinity, 0, Infinity, 50, 0, 0, 100, Infinity);
       ctx.drawImage(red, Infinity, 0, 100, Infinity, 0, 0, 100, 50);
       ctx.drawImage(red, Infinity, 0, 100, Infinity, Infinity, 0, 100, 50);
       ctx.drawImage(red, Infinity, 0, 100, Infinity, Infinity, Infinity, 100, 50);
       ctx.drawImage(red, Infinity, 0, 100, Infinity, Infinity, Infinity, Infinity, 50);
       ctx.drawImage(red, Infinity, 0, 100, Infinity, Infinity, Infinity, Infinity, Infinity);
       ctx.drawImage(red, Infinity, 0, 100, Infinity, Infinity, Infinity, 100, Infinity);
       ctx.drawImage(red, Infinity, 0, 100, Infinity, Infinity, 0, Infinity, 50);
       ctx.drawImage(red, Infinity, 0, 100, Infinity, Infinity, 0, Infinity, Infinity);
       ctx.drawImage(red, Infinity, 0, 100, Infinity, Infinity, 0, 100, Infinity);
       ctx.drawImage(red, Infinity, 0, 100, Infinity, 0, Infinity, 100, 50);
       ctx.drawImage(red, Infinity, 0, 100, Infinity, 0, Infinity, Infinity, 50);
       ctx.drawImage(red, Infinity, 0, 100, Infinity, 0, Infinity, Infinity, Infinity);
       ctx.drawImage(red, Infinity, 0, 100, Infinity, 0, Infinity, 100, Infinity);
       ctx.drawImage(red, Infinity, 0, 100, Infinity, 0, 0, Infinity, 50);
       ctx.drawImage(red, Infinity, 0, 100, Infinity, 0, 0, Infinity, Infinity);
       ctx.drawImage(red, Infinity, 0, 100, Infinity, 0, 0, 100, Infinity);
       ctx.drawImage(red, Infinity, 0, 100, 50, Infinity, 0, 100, 50);
       ctx.drawImage(red, Infinity, 0, 100, 50, Infinity, Infinity, 100, 50);
       ctx.drawImage(red, Infinity, 0, 100, 50, Infinity, Infinity, Infinity, 50);
       ctx.drawImage(red, Infinity, 0, 100, 50, Infinity, Infinity, Infinity, Infinity);
       ctx.drawImage(red, Infinity, 0, 100, 50, Infinity, Infinity, 100, Infinity);
       ctx.drawImage(red, Infinity, 0, 100, 50, Infinity, 0, Infinity, 50);
       ctx.drawImage(red, Infinity, 0, 100, 50, Infinity, 0, Infinity, Infinity);
       ctx.drawImage(red, Infinity, 0, 100, 50, Infinity, 0, 100, Infinity);
       ctx.drawImage(red, Infinity, 0, 100, 50, 0, Infinity, 100, 50);
       ctx.drawImage(red, Infinity, 0, 100, 50, 0, Infinity, Infinity, 50);
       ctx.drawImage(red, Infinity, 0, 100, 50, 0, Infinity, Infinity, Infinity);
       ctx.drawImage(red, Infinity, 0, 100, 50, 0, Infinity, 100, Infinity);
       ctx.drawImage(red, Infinity, 0, 100, 50, 0, 0, Infinity, 50);
       ctx.drawImage(red, Infinity, 0, 100, 50, 0, 0, Infinity, Infinity);
       ctx.drawImage(red, Infinity, 0, 100, 50, 0, 0, 100, Infinity);
       ctx.drawImage(red, 0, Infinity, Infinity, 50, 0, 0, 100, 50);
       ctx.drawImage(red, 0, Infinity, Infinity, Infinity, 0, 0, 100, 50);
       ctx.drawImage(red, 0, Infinity, Infinity, Infinity, Infinity, 0, 100, 50);
       ctx.drawImage(red, 0, Infinity, Infinity, Infinity, Infinity, Infinity, 100, 50);
       ctx.drawImage(red, 0, Infinity, Infinity, Infinity, Infinity, Infinity, Infinity, 50);
       ctx.drawImage(red, 0, Infinity, Infinity, Infinity, Infinity, Infinity, Infinity, Infinity);
       ctx.drawImage(red, 0, Infinity, Infinity, Infinity, Infinity, Infinity, 100, Infinity);
       ctx.drawImage(red, 0, Infinity, Infinity, Infinity, Infinity, 0, Infinity, 50);
       ctx.drawImage(red, 0, Infinity, Infinity, Infinity, Infinity, 0, Infinity, Infinity);
       ctx.drawImage(red, 0, Infinity, Infinity, Infinity, Infinity, 0, 100, Infinity);
       ctx.drawImage(red, 0, Infinity, Infinity, Infinity, 0, Infinity, 100, 50);
       ctx.drawImage(red, 0, Infinity, Infinity, Infinity, 0, Infinity, Infinity, 50);
       ctx.drawImage(red, 0, Infinity, Infinity, Infinity, 0, Infinity, Infinity, Infinity);
       ctx.drawImage(red, 0, Infinity, Infinity, Infinity, 0, Infinity, 100, Infinity);
       ctx.drawImage(red, 0, Infinity, Infinity, Infinity, 0, 0, Infinity, 50);
       ctx.drawImage(red, 0, Infinity, Infinity, Infinity, 0, 0, Infinity, Infinity);
       ctx.drawImage(red, 0, Infinity, Infinity, Infinity, 0, 0, 100, Infinity);
       ctx.drawImage(red, 0, Infinity, Infinity, 50, Infinity, 0, 100, 50);
       ctx.drawImage(red, 0, Infinity, Infinity, 50, Infinity, Infinity, 100, 50);
       ctx.drawImage(red, 0, Infinity, Infinity, 50, Infinity, Infinity, Infinity, 50);
       ctx.drawImage(red, 0, Infinity, Infinity, 50, Infinity, Infinity, Infinity, Infinity);
       ctx.drawImage(red, 0, Infinity, Infinity, 50, Infinity, Infinity, 100, Infinity);
       ctx.drawImage(red, 0, Infinity, Infinity, 50, Infinity, 0, Infinity, 50);
       ctx.drawImage(red, 0, Infinity, Infinity, 50, Infinity, 0, Infinity, Infinity);
       ctx.drawImage(red, 0, Infinity, Infinity, 50, Infinity, 0, 100, Infinity);
       ctx.drawImage(red, 0, Infinity, Infinity, 50, 0, Infinity, 100, 50);
       ctx.drawImage(red, 0, Infinity, Infinity, 50, 0, Infinity, Infinity, 50);
       ctx.drawImage(red, 0, Infinity, Infinity, 50, 0, Infinity, Infinity, Infinity);
       ctx.drawImage(red, 0, Infinity, Infinity, 50, 0, Infinity, 100, Infinity);
       ctx.drawImage(red, 0, Infinity, Infinity, 50, 0, 0, Infinity, 50);
       ctx.drawImage(red, 0, Infinity, Infinity, 50, 0, 0, Infinity, Infinity);
       ctx.drawImage(red, 0, Infinity, Infinity, 50, 0, 0, 100, Infinity);
       ctx.drawImage(red, 0, Infinity, 100, Infinity, 0, 0, 100, 50);
       ctx.drawImage(red, 0, Infinity, 100, Infinity, Infinity, 0, 100, 50);
       ctx.drawImage(red, 0, Infinity, 100, Infinity, Infinity, Infinity, 100, 50);
       ctx.drawImage(red, 0, Infinity, 100, Infinity, Infinity, Infinity, Infinity, 50);
       ctx.drawImage(red, 0, Infinity, 100, Infinity, Infinity, Infinity, Infinity, Infinity);
       ctx.drawImage(red, 0, Infinity, 100, Infinity, Infinity, Infinity, 100, Infinity);
       ctx.drawImage(red, 0, Infinity, 100, Infinity, Infinity, 0, Infinity, 50);
       ctx.drawImage(red, 0, Infinity, 100, Infinity, Infinity, 0, Infinity, Infinity);
       ctx.drawImage(red, 0, Infinity, 100, Infinity, Infinity, 0, 100, Infinity);
       ctx.drawImage(red, 0, Infinity, 100, Infinity, 0, Infinity, 100, 50);
       ctx.drawImage(red, 0, Infinity, 100, Infinity, 0, Infinity, Infinity, 50);
       ctx.drawImage(red, 0, Infinity, 100, Infinity, 0, Infinity, Infinity, Infinity);
       ctx.drawImage(red, 0, Infinity, 100, Infinity, 0, Infinity, 100, Infinity);
       ctx.drawImage(red, 0, Infinity, 100, Infinity, 0, 0, Infinity, 50);
       ctx.drawImage(red, 0, Infinity, 100, Infinity, 0, 0, Infinity, Infinity);
       ctx.drawImage(red, 0, Infinity, 100, Infinity, 0, 0, 100, Infinity);
       ctx.drawImage(red, 0, Infinity, 100, 50, Infinity, 0, 100, 50);
       ctx.drawImage(red, 0, Infinity, 100, 50, Infinity, Infinity, 100, 50);
       ctx.drawImage(red, 0, Infinity, 100, 50, Infinity, Infinity, Infinity, 50);
       ctx.drawImage(red, 0, Infinity, 100, 50, Infinity, Infinity, Infinity, Infinity);
       ctx.drawImage(red, 0, Infinity, 100, 50, Infinity, Infinity, 100, Infinity);
       ctx.drawImage(red, 0, Infinity, 100, 50, Infinity, 0, Infinity, 50);
       ctx.drawImage(red, 0, Infinity, 100, 50, Infinity, 0, Infinity, Infinity);
       ctx.drawImage(red, 0, Infinity, 100, 50, Infinity, 0, 100, Infinity);
       ctx.drawImage(red, 0, Infinity, 100, 50, 0, Infinity, 100, 50);
       ctx.drawImage(red, 0, Infinity, 100, 50, 0, Infinity, Infinity, 50);
       ctx.drawImage(red, 0, Infinity, 100, 50, 0, Infinity, Infinity, Infinity);
       ctx.drawImage(red, 0, Infinity, 100, 50, 0, Infinity, 100, Infinity);
       ctx.drawImage(red, 0, Infinity, 100, 50, 0, 0, Infinity, 50);
       ctx.drawImage(red, 0, Infinity, 100, 50, 0, 0, Infinity, Infinity);
       ctx.drawImage(red, 0, Infinity, 100, 50, 0, 0, 100, Infinity);
       ctx.drawImage(red, 0, 0, Infinity, Infinity, 0, 0, 100, 50);
       ctx.drawImage(red, 0, 0, Infinity, Infinity, Infinity, 0, 100, 50);
       ctx.drawImage(red, 0, 0, Infinity, Infinity, Infinity, Infinity, 100, 50);
       ctx.drawImage(red, 0, 0, Infinity, Infinity, Infinity, Infinity, Infinity, 50);
       ctx.drawImage(red, 0, 0, Infinity, Infinity, Infinity, Infinity, Infinity, Infinity);
       ctx.drawImage(red, 0, 0, Infinity, Infinity, Infinity, Infinity, 100, Infinity);
       ctx.drawImage(red, 0, 0, Infinity, Infinity, Infinity, 0, Infinity, 50);
       ctx.drawImage(red, 0, 0, Infinity, Infinity, Infinity, 0, Infinity, Infinity);
       ctx.drawImage(red, 0, 0, Infinity, Infinity, Infinity, 0, 100, Infinity);
       ctx.drawImage(red, 0, 0, Infinity, Infinity, 0, Infinity, 100, 50);
       ctx.drawImage(red, 0, 0, Infinity, Infinity, 0, Infinity, Infinity, 50);
       ctx.drawImage(red, 0, 0, Infinity, Infinity, 0, Infinity, Infinity, Infinity);
       ctx.drawImage(red, 0, 0, Infinity, Infinity, 0, Infinity, 100, Infinity);
       ctx.drawImage(red, 0, 0, Infinity, Infinity, 0, 0, Infinity, 50);
       ctx.drawImage(red, 0, 0, Infinity, Infinity, 0, 0, Infinity, Infinity);
       ctx.drawImage(red, 0, 0, Infinity, Infinity, 0, 0, 100, Infinity);
       ctx.drawImage(red, 0, 0, Infinity, 50, Infinity, 0, 100, 50);
       ctx.drawImage(red, 0, 0, Infinity, 50, Infinity, Infinity, 100, 50);
       ctx.drawImage(red, 0, 0, Infinity, 50, Infinity, Infinity, Infinity, 50);
       ctx.drawImage(red, 0, 0, Infinity, 50, Infinity, Infinity, Infinity, Infinity);
       ctx.drawImage(red, 0, 0, Infinity, 50, Infinity, Infinity, 100, Infinity);
       ctx.drawImage(red, 0, 0, Infinity, 50, Infinity, 0, Infinity, 50);
       ctx.drawImage(red, 0, 0, Infinity, 50, Infinity, 0, Infinity, Infinity);
       ctx.drawImage(red, 0, 0, Infinity, 50, Infinity, 0, 100, Infinity);
       ctx.drawImage(red, 0, 0, Infinity, 50, 0, Infinity, 100, 50);
       ctx.drawImage(red, 0, 0, Infinity, 50, 0, Infinity, Infinity, 50);
       ctx.drawImage(red, 0, 0, Infinity, 50, 0, Infinity, Infinity, Infinity);
       ctx.drawImage(red, 0, 0, Infinity, 50, 0, Infinity, 100, Infinity);
       ctx.drawImage(red, 0, 0, Infinity, 50, 0, 0, Infinity, 50);
       ctx.drawImage(red, 0, 0, Infinity, 50, 0, 0, Infinity, Infinity);
       ctx.drawImage(red, 0, 0, Infinity, 50, 0, 0, 100, Infinity);
       ctx.drawImage(red, 0, 0, 100, Infinity, Infinity, 0, 100, 50);
       ctx.drawImage(red, 0, 0, 100, Infinity, Infinity, Infinity, 100, 50);
       ctx.drawImage(red, 0, 0, 100, Infinity, Infinity, Infinity, Infinity, 50);
       ctx.drawImage(red, 0, 0, 100, Infinity, Infinity, Infinity, Infinity, Infinity);
       ctx.drawImage(red, 0, 0, 100, Infinity, Infinity, Infinity, 100, Infinity);
       ctx.drawImage(red, 0, 0, 100, Infinity, Infinity, 0, Infinity, 50);
       ctx.drawImage(red, 0, 0, 100, Infinity, Infinity, 0, Infinity, Infinity);
       ctx.drawImage(red, 0, 0, 100, Infinity, Infinity, 0, 100, Infinity);
       ctx.drawImage(red, 0, 0, 100, Infinity, 0, Infinity, 100, 50);
       ctx.drawImage(red, 0, 0, 100, Infinity, 0, Infinity, Infinity, 50);
       ctx.drawImage(red, 0, 0, 100, Infinity, 0, Infinity, Infinity, Infinity);
       ctx.drawImage(red, 0, 0, 100, Infinity, 0, Infinity, 100, Infinity);
       ctx.drawImage(red, 0, 0, 100, Infinity, 0, 0, Infinity, 50);
       ctx.drawImage(red, 0, 0, 100, Infinity, 0, 0, Infinity, Infinity);
       ctx.drawImage(red, 0, 0, 100, Infinity, 0, 0, 100, Infinity);
       ctx.drawImage(red, 0, 0, 100, 50, Infinity, Infinity, 100, 50);
       ctx.drawImage(red, 0, 0, 100, 50, Infinity, Infinity, Infinity, 50);
       ctx.drawImage(red, 0, 0, 100, 50, Infinity, Infinity, Infinity, Infinity);
       ctx.drawImage(red, 0, 0, 100, 50, Infinity, Infinity, 100, Infinity);
       ctx.drawImage(red, 0, 0, 100, 50, Infinity, 0, Infinity, 50);
       ctx.drawImage(red, 0, 0, 100, 50, Infinity, 0, Infinity, Infinity);
       ctx.drawImage(red, 0, 0, 100, 50, Infinity, 0, 100, Infinity);
       ctx.drawImage(red, 0, 0, 100, 50, 0, Infinity, Infinity, 50);
       ctx.drawImage(red, 0, 0, 100, 50, 0, Infinity, Infinity, Infinity);
       ctx.drawImage(red, 0, 0, 100, 50, 0, Infinity, 100, Infinity);
       ctx.drawImage(red, 0, 0, 100, 50, 0, 0, Infinity, Infinity);
       comparePixel(ctx, 50,25, 0,255,0,255);

   }

   function test_negative(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       loadImages(canvas);


       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);
       ctx.drawImage('ggrr-256x256.png', 100, 78, 50, 50, 0, 50, 50, -50);
       ctx.drawImage('ggrr-256x256.png', 100, 128, 50, -50, 100, 50, -50, -50);
//           comparePixel(ctx, 1,1, 0,255,0,255,2);
//           comparePixel(ctx, 1,48, 0,255,0,255,2);
//           comparePixel(ctx, 98,1, 0,255,0,255,2);
//           comparePixel(ctx, 98,48, 0,255,0,255,2);
//           comparePixel(ctx, 48,1, 0,255,0,255,2);
//           comparePixel(ctx, 48,48, 0,255,0,255,2);
//           comparePixel(ctx, 51,1, 0,255,0,255,2);
//           comparePixel(ctx, 51,48, 0,255,0,255,2);
//           comparePixel(ctx, 25,25, 0,255,0,255,2);
//           comparePixel(ctx, 75,25, 0,255,0,255,2);

       ctx.reset();
       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);
       ctx.drawImage('ggrr-256x256.png', 0, 178, 50, -100, 0, 0, 50, 100);
       ctx.drawImage('ggrr-256x256.png', 0, 78, 50, 100, 50, 100, 50, -100);
//           comparePixel(ctx, 1,1, 0,255,0,255,2);
//           comparePixel(ctx, 1,48, 0,255,0,255,2);
//           comparePixel(ctx, 98,1, 0,255,0,255,2);
//           comparePixel(ctx, 98,48, 0,255,0,255,2);
//           comparePixel(ctx, 48,1, 0,255,0,255,2);
//           comparePixel(ctx, 48,48, 0,255,0,255,2);
//           comparePixel(ctx, 51,1, 0,255,0,255,2);
//           comparePixel(ctx, 51,48, 0,255,0,255,2);
//           comparePixel(ctx, 25,25, 0,255,0,255,2);
//           comparePixel(ctx, 75,25, 0,255,0,255,2);

       ctx.reset();
       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);
       ctx.drawImage('ggrr-256x256.png', 100, 78, -100, 50, 0, 0, 50, 50);
       ctx.drawImage('ggrr-256x256.png', 100, 128, -100, -50, 50, 0, 50, 50);
//           comparePixel(ctx, 1,1, 0,255,0,255,2);
//           comparePixel(ctx, 1,48, 0,255,0,255,2);
//           comparePixel(ctx, 98,1, 0,255,0,255,2);
//           comparePixel(ctx, 98,48, 0,255,0,255,2);
//           comparePixel(ctx, 48,1, 0,255,0,255,2);
//           comparePixel(ctx, 48,48, 0,255,0,255,2);
//           comparePixel(ctx, 51,1, 0,255,0,255,2);
//           comparePixel(ctx, 51,48, 0,255,0,255,2);
//           comparePixel(ctx, 25,25, 0,255,0,255,2);
//           comparePixel(ctx, 75,25, 0,255,0,255,2);

   }

   function test_canvas(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       loadImages(canvas);

       var canvas2 = Qt.createQmlObject("import QtQuick 2.0; Canvas{renderTarget:Canvas.Image; renderStrategy:Canvas.Immediate}", canvas);
       canvas2.width = 100;
       canvas2.height = 50;
       waitForRendering(canvas2);
       var ctx2 = canvas2.getContext('2d');
       ctx2.fillStyle = '#0f0';
       ctx2.fillRect(0, 0, 100, 50);

       ctx.fillStyle = '#f00';
       ctx.drawImage(canvas2, 0, 0);

       //comparePixel(ctx, 0,0, 0,255,0,255,2);
       //comparePixel(ctx, 99,0, 0,255,0,255,2);
       //comparePixel(ctx, 0,49, 0,255,0,255,2);
       //comparePixel(ctx, 99,49, 0,255,0,255,2);

   }

   function test_broken(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       loadImages(canvas);

       var img = 'broken.png';
       verify(!img.complete);
       ctx.drawImage(img, 0, 0);
   }

   function test_alpha(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       loadImages(canvas);

       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);
       ctx.globalAlpha = 0;
       ctx.drawImage('red.png', 0, 0);
       comparePixel(ctx, 50,25, 0,255,0,255, 2);

   }
   function test_multiple_painting(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       loadImages(canvas);

       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);
       ctx.drawImage('red.png', 0, 0, 50, 50);
       ctx.drawImage('red.png', 50, 0, 100, 50);
       comparePixel(ctx, 25,25, 255,0,0,255, 2);
       comparePixel(ctx, 75,25, 255,0,0,255, 2);
   }
}
