uniform sampler2D yuvTexture; // YUYV macropixel texture passed as RGBA format
uniform mediump float imageWidth; // The YUYV texture appears to the shader with 1/2 the image width since we use the RGBA format to pass YUYV
uniform mediump mat4 colorMatrix;
uniform lowp float opacity;

varying highp vec2 qt_TexCoord;

void main()
{
   // For Y0 U0 Y1 V0  macropixel, lookup Y0 or Y1 based on whether
   // the original texture x coord is even or odd.
   mediump float Y;
   if (fract(floor(qt_TexCoord.x * imageWidth + 0.5) / 2.0) > 0.0)
       Y = texture2D(yuvTexture, qt_TexCoord).b;       // odd so choose Y1
   else
       Y = texture2D(yuvTexture, qt_TexCoord).r;       // even so choose Y0
   mediump float Cb = texture2D(yuvTexture, qt_TexCoord).g;
   mediump float Cr = texture2D(yuvTexture, qt_TexCoord).a;

   mediump vec4 color = vec4(Y, Cb, Cr, 1.0);
   gl_FragColor = colorMatrix * color * opacity;
}
