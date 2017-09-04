uniform sampler2D yuvTexture; // UYVY macropixel texture passed as RGBA format
uniform mediump float imageWidth; // The UYVY texture appears to the shader with 1/2 the image width since we use the RGBA format to pass UYVY
uniform mediump mat4 colorMatrix;
uniform lowp float opacity;

varying highp vec2 qt_TexCoord;

void main()
{
   // For U0 Y0 V0 Y1 macropixel, lookup Y0 or Y1 based on whether
   // the original texture x coord is even or odd.
   mediump float Y;
   if (fract(floor(qt_TexCoord.x * imageWidth + 0.5) / 2.0) > 0.0)
       Y = texture2D(yuvTexture, qt_TexCoord).a;       // odd so choose Y1
   else
       Y = texture2D(yuvTexture, qt_TexCoord).g;       // even so choose Y0
   mediump float Cb = texture2D(yuvTexture, qt_TexCoord).r;
   mediump float Cr = texture2D(yuvTexture, qt_TexCoord).b;

   mediump vec4 color = vec4(Y, Cb, Cr, 1.0);
   gl_FragColor = colorMatrix * color * opacity;
}
