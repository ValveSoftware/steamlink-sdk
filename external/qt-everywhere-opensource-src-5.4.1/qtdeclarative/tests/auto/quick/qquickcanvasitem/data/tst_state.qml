import QtQuick 2.0

CanvasTestCase {
   id:testCase
   name: "state"
   function init_data() { return testData("2d"); }
   function test_bitmap(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);
       ctx.save();
       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);
       ctx.restore();
       comparePixel(ctx, 50,25, 0,255,0,255);
       canvas.destroy()
  }
   function test_clip(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();

       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);
       ctx.save();
       ctx.rect(0, 0, 1, 1);
       ctx.clip();
       ctx.restore();
       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);
       comparePixel(ctx, 50,25, 0,255,0,255);
       canvas.destroy()
   }
   function test_fillStyle(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       // Test that restore() undoes any modifications
       var old = ctx.fillStyle;
       ctx.save();
       ctx.fillStyle = "#ff0000";
       ctx.restore();
       compare(ctx.fillStyle, old);

       // Also test that save() doesn't modify the values
       ctx.fillStyle = "#ff0000";
       old = ctx.fillStyle;
           // we're not interested in failures caused by get(set(x)) != x (e.g.
           // from rounding), so compare against 'old' instead of against "#ff0000"
       ctx.save();
       compare(ctx.fillStyle, old);
       ctx.restore();
       canvas.destroy()
   }
   function test_font(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();

       // Test that restore() undoes any modifications
       var old = ctx.font;
       ctx.save();
       ctx.font = "25px serif";
       ctx.restore();
       compare(ctx.font, old);

       // Also test that save() doesn't modify the values
       ctx.font = "25px serif";
       old = ctx.font;
           // we're not interested in failures caused by get(set(x)) != x (e.g.
           // from rounding), so compare against 'old' instead of against "25px serif"
       ctx.save();
       compare(ctx.font, old);
       ctx.restore();
       canvas.destroy()
   }
   function test_globalAlpha(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();

       // Test that restore() undoes any modifications
       var old = ctx.globalAlpha;
       ctx.save();
       ctx.globalAlpha = 0.5;
       ctx.restore();
       compare(ctx.globalAlpha, old);

       // Also test that save() doesn't modify the values
       ctx.globalAlpha = 0.5;
       old = ctx.globalAlpha;
           // we're not interested in failures caused by get(set(x)) != x (e.g.
           // from rounding), so compare against 'old' instead of against 0.5
       ctx.save();
       compare(ctx.globalAlpha, old);
       ctx.restore();
       canvas.destroy()
    }
   function test_globalCompositeOperation(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();

       // Test that restore() undoes any modifications
       var old = ctx.globalCompositeOperation;
       ctx.save();
       ctx.globalCompositeOperation = "copy";
       ctx.restore();
       compare(ctx.globalCompositeOperation, old);

       // Also test that save() doesn't modify the values
       ctx.globalCompositeOperation = "copy";
       old = ctx.globalCompositeOperation;
           // we're not interested in failures caused by get(set(x)) != x (e.g.
           // from rounding), so compare against 'old' instead of against "copy"
       ctx.save();
       compare(ctx.globalCompositeOperation, old);
       ctx.restore();
       canvas.destroy()
   }
   function test_lineCap(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();

       // Test that restore() undoes any modifications
       var old = ctx.lineCap;
       ctx.save();
       ctx.lineCap = "round";
       ctx.restore();
       compare(ctx.lineCap, old);

       // Also test that save() doesn't modify the values
       ctx.lineCap = "round";
       old = ctx.lineCap;
           // we're not interested in failures caused by get(set(x)) != x (e.g.
           // from rounding), so compare against 'old' instead of against "round"
       ctx.save();
       compare(ctx.lineCap, old);
       ctx.restore();
       canvas.destroy()
   }
   function test_lineJoin(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();

       // Test that restore() undoes any modifications
       var old = ctx.lineJoin;
       ctx.save();
       ctx.lineJoin = "round";
       ctx.restore();
       compare(ctx.lineJoin, old);

       // Also test that save() doesn't modify the values
       ctx.lineJoin = "round";
       old = ctx.lineJoin;
           // we're not interested in failures caused by get(set(x)) != x (e.g.
           // from rounding), so compare against 'old' instead of against "round"
       ctx.save();
       compare(ctx.lineJoin, old);
       ctx.restore();
       canvas.destroy()
   }
   function test_lineWidth(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();

       // Test that restore() undoes any modifications
       var old = ctx.lineJoin;
       ctx.save();
       ctx.lineJoin = "round";
       ctx.restore();
       compare(ctx.lineJoin, old, "ctx.lineJoin", "old");

       // Also test that save() doesn't modify the values
       ctx.lineJoin = "round";
       old = ctx.lineJoin;
           // we're not interested in failures caused by get(set(x)) != x (e.g.
           // from rounding), so compare against 'old' instead of against "round"
       ctx.save();
       compare(ctx.lineJoin, old);
       ctx.restore();
       canvas.destroy()
   }
   function test_miterLimit(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();

       // Test that restore() undoes any modifications
       var old = ctx.miterLimit;
       ctx.save();
       ctx.miterLimit = 0.5;
       ctx.restore();
       compare(ctx.miterLimit, old);

       // Also test that save() doesn't modify the values
       ctx.miterLimit = 0.5;
       old = ctx.miterLimit;
           // we're not interested in failures caused by get(set(x)) != x (e.g.
           // from rounding), so compare against 'old' instead of against 0.5
       ctx.save();
       compare(ctx.miterLimit, old);
       ctx.restore();
       canvas.destroy()
   }
   function test_path(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();

       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);
       ctx.save();
       ctx.rect(0, 0, 100, 50);
       ctx.restore();
       ctx.fillStyle = '#0f0';
       ctx.fill();
       comparePixel(ctx, 50,25, 0,255,0,255);
       canvas.destroy()
   }
   function test_shadow(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();

       // Test that restore() undoes any modifications
       var old = ctx.shadowBlur;
       ctx.save();
       ctx.shadowBlur = 5;
       ctx.restore();
       compare(ctx.shadowBlur, old);

       // Also test that save() doesn't modify the values
       ctx.shadowBlur = 5;
       old = ctx.shadowBlur;
           // we're not interested in failures caused by get(set(x)) != x (e.g.
           // from rounding), so compare against 'old' instead of against 5
       ctx.save();
       compare(ctx.shadowBlur, old);
       ctx.restore();

       // Test that restore() undoes any modifications
       var old = ctx.shadowColor;
       ctx.save();
       ctx.shadowColor = "#ff0000";
       ctx.restore();
       compare(ctx.shadowColor, old);

       // Also test that save() doesn't modify the values
       ctx.shadowColor = "#ff0000";
       old = ctx.shadowColor;
           // we're not interested in failures caused by get(set(x)) != x (e.g.
           // from rounding), so compare against 'old' instead of against "#ff0000"
       ctx.save();
       compare(ctx.shadowColor, old);
       ctx.restore();

       // Test that restore() undoes any modifications
       var old = ctx.shadowOffsetX;
       ctx.save();
       ctx.shadowOffsetX = 5;
       ctx.restore();
       compare(ctx.shadowOffsetX, old);

       // Also test that save() doesn't modify the values
       ctx.shadowOffsetX = 5;
       old = ctx.shadowOffsetX;
           // we're not interested in failures caused by get(set(x)) != x (e.g.
           // from rounding), so compare against 'old' instead of against 5
       ctx.save();
       compare(ctx.shadowOffsetX, old);
       ctx.restore();

       // Test that restore() undoes any modifications
       var old = ctx.shadowOffsetY;
       ctx.save();
       ctx.shadowOffsetY = 5;
       ctx.restore();
       compare(ctx.shadowOffsetY, old);

       // Also test that save() doesn't modify the values
       ctx.shadowOffsetY = 5;
       old = ctx.shadowOffsetY;
           // we're not interested in failures caused by get(set(x)) != x (e.g.
           // from rounding), so compare against 'old' instead of against 5
       ctx.save();
       compare(ctx.shadowOffsetY, old);
       ctx.restore();
       canvas.destroy()
   }
   function test_stack(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();

       ctx.lineWidth = 1;
       ctx.save();
       ctx.lineWidth = 2;
       ctx.save();
       ctx.lineWidth = 3;
       compare(ctx.lineWidth, 3);
       ctx.restore();
       compare(ctx.lineWidth, 2);
       ctx.restore();
       compare(ctx.lineWidth, 1);

       var limit = 512;
       for (var i = 1; i < limit; ++i)
       {
           ctx.save();
           ctx.lineWidth = i;
       }
       for (var i = limit-1; i > 0; --i)
       {
           testCase.compare(ctx.lineWidth, i); //strange javascript error here
           ctx.restore();
       }

       for (var i = 0; i < 16; ++i)
           ctx.restore();
       ctx.lineWidth = 0.5;
       ctx.restore();
       compare(ctx.lineWidth, 0.5);
       canvas.destroy()
   }
   function test_strokeStyle(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();

       // Test that restore() undoes any modifications
       var old = ctx.strokeStyle;
       ctx.save();
       ctx.strokeStyle = "#ff0000";
       ctx.restore();
       compare(ctx.strokeStyle, old);

       // Also test that save() doesn't modify the values
       ctx.strokeStyle = "#ff0000";
       old = ctx.strokeStyle;
           // we're not interested in failures caused by get(set(x)) != x (e.g.
           // from rounding), so compare against 'old' instead of against "#ff0000"
       ctx.save();
       compare(ctx.strokeStyle, old);
       ctx.restore();
       canvas.destroy()
   }

   function test_text(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();

       // Test that restore() undoes any modifications
       var old = ctx.textAlign;
       ctx.save();
       ctx.textAlign = "center";
       ctx.restore();
       compare(ctx.textAlign, old);

       // Also test that save() doesn't modify the values
       ctx.textAlign = "center";
       old = ctx.textAlign;
           // we're not interested in failures caused by get(set(x)) != x (e.g.
           // from rounding), so compare against 'old' instead of against "center"
       ctx.save();
       compare(ctx.textAlign, old);
       ctx.restore();

       // Test that restore() undoes any modifications
       var old = ctx.textBaseline;
       ctx.save();
       ctx.textBaseline = "bottom";
       ctx.restore();
       compare(ctx.textBaseline, old);

       // Also test that save() doesn't modify the values
       ctx.textBaseline = "bottom";
       old = ctx.textBaseline;
           // we're not interested in failures caused by get(set(x)) != x (e.g.
           // from rounding), so compare against 'old' instead of against "bottom"
       ctx.save();
       compare(ctx.textBaseline, old);
       ctx.restore();
       canvas.destroy()
   }

   function test_transform(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();

       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);
       ctx.save();
       ctx.translate(200, 0);
       ctx.restore();
       ctx.fillStyle = '#f00';
       ctx.fillRect(-200, 0, 100, 50);
       comparePixel(ctx, 50,25, 0,255,0,255);
       canvas.destroy()
   }
}
