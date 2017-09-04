/*****************************************************************************************
* Copyright (c) 2006 Hewlett-Packard Development Company, L.P.
* Permission is hereby granted, free of charge, to any person obtaining a copy of
* this software and associated documentation files (the "Software"), to deal in
* the Software without restriction, including without limitation the rights to use,
* copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
* Software, and to permit persons to whom the Software is furnished to do so,
* subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all 
* copies or substantial portions of the Software.
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
 * $LastChangedDate:  $
 * $Revision:  $
 * $Author:  $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Definitions for SubStroke feature extractor module
 *
 * CONTENTS:
 *
 * AUTHOR:     Tanmay Mondal
 *
 * DATE:       February 2009
 * CHANGE HISTORY:
 * Author      Date           Description of change
 ************************************************************************/
#ifndef __SUBSTROKESHAPEFEATUREEXTRACTOR_H
#define __SUBSTROKESHAPEFEATUREEXTRACTOR_H

#define SUPPORTED_MIN_VERSION "3.0.0"

// delimiter of the slope of extracted substroke
#define SUBSTROKES_ANGLE_DELIMITER -999.0

#define SUBSTROKES_LENGTH_REJECT_THRESHOLD 0.001

#define STROKE_SEGMENT_VALUE 0

#define SINGLE_SUBSTROKES 1

#define ANGLE_HIGHER_LIMIT_1  22.5

#define ANGLE_HIGHER_LIMIT_2  67.5

#define ANGLE_HIGHER_LIMIT_3  112.5

#define ANGLE_HIGHER_LIMIT_4  157.5

#define ANGLE_HIGHER_LIMIT_5  202.5

#define ANGLE_HIGHER_LIMIT_6  247.5

#define ANGLE_HIGHER_LIMIT_7  292.5

#define ANGLE_HIGHER_LIMIT_8  337.5

#define PI_DEGREE                  180.0

#include "LTKShapeFeatureExtractor.h"

class SubStrokeShapeFeatureExtractor : public LTKShapeFeatureExtractor
{

public:
     /** @brief Constructor for the SubStroke feature extractor
      * Gets the cfg file path from the contorInfo
      * Reads the cfg variables and poputlates the member variables
      * @param controlInfo: LTKControlInfo : The control information
      * @return no return value as it is a constructor
      */

     SubStrokeShapeFeatureExtractor(const LTKControlInfo& controlInfo);

    int extractFeatures(const LTKTraceGroup& inTraceGroup,
                          vector<LTKShapeFeaturePtr>& outFeatureVec);

    LTKShapeFeaturePtr getShapeFeatureInstance();

private:

     //store extracted sub-strokes
     struct subStrokePoint
     {
          float X;  // x point
          float Y;  // y point
          bool penUp; // If point is penup then penUp = 1 else penUp =0
     };

     /** extract feature from sub strokes
     */
     int extractFeaturesFromSubStroke(const vector<struct subStrokePoint>& inSubStrokeVector,
                                             vector<float>& outSlope,
                                             vector<float>& outLength,
                                             vector<float>& outCenterOfGravity);

     /** extract the sub strokes from the preprocessed trace group
     */
     int extractSubStrokesFromInk(const LTKTraceGroup& inTraceGroup, vector<struct subStrokePoint>& outSubStrokeVector);

     /** @brief claculate the angle
     * inDeltaX,inDeltaY : float : the difference of x[i] and x[i+1] and difference of y[i] and y[i+1] of ink point
     * float : the angle made with x axis and the point
     */
     int computeSlope(const float inDeltaX, const float inDeltaY, float& outSlope);

     /** decide the octant of the point along the ink
     * slope : float : value of the angle
     * int : the octant code
     */
     int getDirectionCode(const float inSlope, int& outDirectionCode);

     /** break the stroke into substrokes
     * infirstSlope, insecondSlope : float : value of the slope
     * int : SUCCESS : FAILURE
     */
     int canSegmentStrokes(const float inFirstSlope, const float inSecondSlope, bool& outSegment);

     /** compute the slope of each point present in the tace
     * inTrace : LTKTrace : value of a trace
     * int : SUCCESS : FAILURE : errorcode
     */
     int getSlopeFromTrace(const LTKTrace& inTrace, vector<float>& outSlopeVector);

};

#endif //#ifndef __SUBSTROKESHAPEFEATUREEXTRACTOR_H
