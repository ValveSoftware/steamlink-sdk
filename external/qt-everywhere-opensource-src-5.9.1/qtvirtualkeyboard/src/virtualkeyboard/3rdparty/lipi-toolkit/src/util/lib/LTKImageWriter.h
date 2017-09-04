/*****************************************************************************************
* Copyright (c) 2007 Hewlett-Packard Development Company, L.P.
* Permission is hereby granted, free of charge, to any person obtaining a copy of
* this software and associated documentation files (the "Software"), to deal in
* the Software without restriction, including without limitation the rights to use,
* copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
* Software, and to permit persons to whom the Software is furnished to do so,
* subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
* PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
* HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
* CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
* OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*****************************************************************************************/

/************************************************************************
 * SVN MACROS
 *
 * $LastChangedDate: 2007-10-08 22:10:54 +0530 (Mon, 08 Oct 2007) $
 * $Revision: 252 $
 * $Author: bharatha $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Declarations of Image Writer module
 *
 * CONTENTS:
 *
 * AUTHOR:     Bharath A
 *
 * DATE:       February 22, 2005
 * CHANGE HISTORY:
 * Author      Date           Description of change
 ************************************************************************/
#ifndef __LTKImageWriter_H
#define __LTKImageWriter_H

#include <limits>
#include <math.h>
#include "LTKInc.h"
#include "LTKMacros.h"
#include "LTKTypes.h"
#include "LTKLoggerUtil.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#define SUPPORTED_MIN_VERSION "4.0.0"

//class LTKPreprocessorInterface;
class LTKTraceGroup;
class LTKScreenContext;
class LTKTrace;

//typedef LTKPreprocessorInterface* (*FN_PTR_CREATELTKLIPIPREPROCESSOR)(LTKControlInfo);

/**
* @class LTKImageWriter
* <p>This class converts LTKTraceGroup or LTKTrace to images. Inputs could be objects of LTKTraceGroup,LTKTrace or path to the file containing the LTKTraceGroup.</p>
* <p>The output images could also display the trace order if the offset is not equal to zero.</p>
*/

class LTKImageWriter{

private:


     unsigned char *m_pixels;           //pointer to the array of pixels

     int m_width;                            //width of the image

     int m_height;                           //height of the image

     float m_xMin,m_yMin,m_xMax,m_yMax;  //Min and Max of tracegroups

     bool m_showBB;                          //flag to show bounding box

     bool m_showSP;                          //flag to show starting point of each stroke

     unsigned char m_red;                    //Red value of starting color
     unsigned char m_green;                  //Green value of starting color
     unsigned char m_blue;                   //Blue value of starting color

     unsigned char m_altRed;                 //Red value of alternate color
     unsigned char m_altGreen;               //Green value of alternate color
     unsigned char m_altBlue;           //Blue value of alternate color

     int m_offset;                           //offset value between strokes

     string xChannelstr, yChannelstr;

public:
     /**
      * @name Constructors and Destructor
      */
     // @{

     /**
      * Default Constructor
      */

     LTKImageWriter();

     /**
      * Destructor
      */
     ~LTKImageWriter();

     /**
      * @name Image writing functions
      */
     // @{

     /**
      * The function draws LTKTraceGroup to image file with specified width,height,color and offset. If offset not equal to zero, the image would be in the trace order.
      *
      * @param traceGroup the trace group that is to be drawn
      * @param imgFileName name of the file that is to be created to draw the image
      * @param imgWidth width of the image
      * @param imgHeight height of the image
      *
      * @return void
      */
     void drawLTKTraceGroupToImage(const LTKTraceGroup& traceGroup,
                              const string imgFileName,int imgWidth,int imgHeight);



     /**
      * The function draws LTKTraceGroup to image file with specified color and offset. The width and height would be determined from the trace group. If offset not equal to zero, the image would be in the trace order.
      *
      * @param traceGroup the trace group that is to be drawn
      * @param imgFileName name of the file that is to be created to draw the image
      *
      * @return void
      */
     void drawLTKTraceGroupToImage(const LTKTraceGroup& traceGroup, const string imgFileName);



     /**
      * The function draws LTKTraceGroup to image file with specified color and offset. The size specified is the bound on larger dimension and the trace group is normalized to maintain the aspect ratio.
      *
      * @param traceGroup the trace group that is to be drawn
      * @param imgFileName name of the file that is to be created to draw the image
      * @param size bound on larger dimension
      *
      * @return void
      */
     void drawLTKTraceGroupToImage(const LTKTraceGroup& traceGroup,const string imgFileName,int size);



     /**
      * The function draws LTKTraceGroup to image file with specified color and offset. The size specified is the bound on larger dimension and the trace group is normalized to maintain the aspect ratio.
      * The bounding box of the image is also drawn to show relative position.
      *
      * @param traceGroup the trace group that is to be drawn
      * @param screenContext the reference to screen Context for determining the bounding box.
      * @param imgFileName name of the file that is to be created to draw the image
      * @param size bound on larger dimension
      * @return void
      */
     void drawLTKTraceGroupToImageWithBB(const LTKTraceGroup& traceGroup,const LTKScreenContext& screenContext,
                                                       const string imgFileName,int size);



     /**
      * The function creates LTKTraceGroup from the specified data file and draws it to image file with specified width,height,color and offset. If offset not equal to zero, the image would be in the trace order.
      *
      * @param fileName path to the data file
      * @param imgFileName name of the file that is to be created to draw the image
      * @param imgWidth width of the image
      * @param imgHeight height of the image
      *
      * @return void
      */
     void drawRawInkFileToImage(const string fileName,const string imgFileName,
                                                       int imgWidth,int imgHeight);



     /**
      * The function creates LTKTraceGroup from the specified data file and draws it to image file with specified color and offset. The width and height would be determined from the trace group.
      *
      * @param fileName path to the data file
      * @param imgFileName name of the file that is to be created to draw the image
      *
      * @return void
      */
     void drawRawInkFileToImage(const string fileName,const string imgFileName);



     /**
      * The function creates LTKTraceGroup from the specified data file and draws it to image file with specified color and offset. The size specified is the bound on larger dimension and the trace group is normalized to maintain the aspect ratio.
      *
      * @param fileName path to the data file
      * @param imgFileName name of the file that is to be created to draw the image
      * @param size bound on larger dimension
      *
      * @return void
      */
     void drawRawInkFileToImage(const string fileName,const string imgFileName,int size);



     /**
      * The function creates LTKTraceGroup from the specified unipen data file and draws it to image file with specified color and offset. The width and height would be determined from the trace group.
      *
      * @param unipenFileName path to the unipen data file
      * @param imgFileName name of the file that is to be created to draw the image
      * @param imgWidth width of the image
      * @param imgHeight height of the image
      *
      * @return void

      */

     void drawUnipenFileToImage(const string unipenFileName,const string imgFileName,int imgWidth,int imgHeight);



     /**
      * The function creates LTKTraceGroup from the specified unipen data file and draws it to image file with specified color and offset. The width and height would be determined from the trace group.
      *
      * @param unipenFileName path to the unipen data file
      * @param imgFileName name of the file that is to be created to draw the image
      *
      * @return void
      */
     void drawUnipenFileToImage(const string& unipenFileName,const string& imgFileName);



     /**
      * The function creates LTKTraceGroup from the specified unipen data file and draws it to image file with specified color and offset. The size specified is the bound on larger dimension and the trace group is normalized to maintain the aspect ratio.
      *
      * @param unipenFileName path to the unipen data file
      * @param imgFileName name of the file that is to be created to draw the image
      * @param size bound on larger dimension
      *
      * @return void
      */
     void drawUnipenFileToImage(const string& unipenFileName,const string& imgFileName,int size);



     /**
      * The function creates LTKTraceGroup from the specified unipen data file and draws it to image file with specified color and offset. The size specified is the bound on larger dimension and the trace group is normalized to maintain the aspect ratio.
      * The bounding box of the image is also drawn to show relative position.
      *
      * @param unipenFileName path to the unipen data file
      * @param imgFileName name of the file that is to be created to draw the image
      * @param size bound on larger dimension
      *
      * @return void
      */


     void drawUnipenFileToImageWithBB(const string& unipenFileName,const string& imgFileName,int size);


     /**
      * Setter method for showing the starting point of each stroke
      *
      * @param toShow flag to show the starting point
      *
      * @return
      */

     void showStartingPoint(bool toShow);

     /**
      * Setter method for color of the starting stroke and subsequent alternate strokes
      *
      * @param red value of Red in RGB combination
      * @param green value of Green in RGB combination
      * @param blue value of Blue in RGB combination
      *
      * @return void
      */

     void setColor(unsigned char red,unsigned char green,unsigned char blue);

     /**
      * Setter method for color of alternate strokes
      *
      * @param altRed value of Red in RGB combination
      * @param altGreen value of Green in RGB combination
      * @param altBlue value of Blue in RGB combination
      *
      * @return void
      */

     void setAlternateColor(unsigned char altRed,unsigned char altGreen,unsigned char altBlue);


     /**
      * Setter method for offset value between strokes
      *
      * @param offset value of offset
      *
      * @return void
      */

     void setOffset(int offset);


private:

     /**
      * @name Preprocessing, trace group ordering, and point,line and rectangle drawing functions.
      */



     /**
      * The function finds the minimum x value of the given trace.
      *
      * @param trace input trace
      *
      * @return minimum value of x
      */

    int findMinXOfTrace(const LTKTrace& trace,float& minX);



     /**
      * The function finds the maximum x value of the given trace.
      *
      * @param trace input trace
      *
      * @return maximum value of x
      */

    int findMaxXOfTrace(const LTKTrace& trace,float& maxX);



     /**
      * The function makes entries in the pixels array for the specified x,y and color.
      *
      * @param x,y coordinates of the point to be drawn
      * @param red,green,blue RGB values of the color of the point
      *
      * @return
      */

     void drawPoint(int x,int y,unsigned char red,unsigned char green,unsigned char blue);



     /**
      * The function draws line between the specified end points.
      *
      * @param x1,y1,x2,y2 coordinates of the end points of the line to be drawn
      * @param red,green,blue RGB values of the color of the line
      *
      * @return
      */

     void drawLine(int x1, int y1, int x2, int y2,unsigned char red,unsigned char green,unsigned char blue);



     /**
      * The function draws hollow rectange with specified color and diagonal end points.
      *
      * @param x1,y1,x2,y2 coordinates of the end points of the diagonal of the rectangle
      * @param red,green,blue RGB values of the color of the lines
      *
      * @return
      */

     void drawRectangle(int x1,int y1,int x2,int y2,unsigned char red,unsigned char green,unsigned char blue);



     /**
      * The function draws a rectange with specified diagonal end points and fills it with the specified color.
      *
      * @param x1,y1,x2,y2 coordinates of the end points of the diagonal of the rectangle
      * @param red,green,blue RGB values of the color of the lines
      *
      * @return
      */

     void fillRectangle(int x1,int y1,int x2,int y2,unsigned char red,unsigned char green,unsigned char blue);



     /**
      * The function offsets the input trace group by specified offset value and in the original trace order.
      *
      * @param traceGroup input trace group
      * @param offsetTraceGroup trace group with offset
      *
      * @return
      */

     void  createTraceOrderInTraceGroup(const LTKTraceGroup& traceGroup,LTKTraceGroup& offsetTraceGroup);

     /**
      * The function writes the given pixel array to the specified BMP file
      *
      * @param fileName name of output bmp file name with extension as 'bmp'
      * @param pixelArray pixel array
      * @param width width of the image
      * @param height height of the image
      *
      * @return
      */

     void drawBMPImage(string fileName,unsigned char* pixelArray,int width,int height);


};
#endif



