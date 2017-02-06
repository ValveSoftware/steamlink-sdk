/*****************************************************************************************
* Copyright (c) 2006 Hewlett-Packard Development Company, L.P.
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
 * $LastChangedDate: 2009-07-01 16:41:37 +0530 (Wed, 01 Jul 2009) $
 * $Revision: 783 $
 * $Author: mnab $
 *
 ************************************************************************/

/************************************************************************
 * FILE DESCR: Definition of LTKPreprocessor which is a library of standard pre-processing functions
 *
 * CONTENTS:
 *
 * AUTHOR:     Balaji R.
 *
 * DATE:       December 23, 2004
 * CHANGE HISTORY:
 * Author      Date           Description of change
 ************************************************************************/

#ifndef __LTKPREPROCESSOR_H
#define __LTKPREPROCESSOR_H

#define SUPPORTED_MIN_VERSION "3.0.0"

#include "LTKCaptureDevice.h"

#include "LTKScreenContext.h"

#include "LTKMacros.h"

#include "LTKInc.h"

#include "LTKPreprocessorInterface.h"

class LTKTrace;
class LTKShapeRecoConfig;

/**
 * @class LTKPreprocessor
 * <p> This class performs pre-processing on a trace group. </p>
 *
 * <p> Different functions performing steps in pre-processing are implemented in different
 * implementation files. Pre-processed trace group is the output of this class module </p>
 * <p> Paramaters used across all pre -processor modules of a shape recognizer will be obtained
 * from the shape recognizer's configuration file. The shape recognizer sets these paramaters in the pre-processor
 * by calling the suitable setter functions defined in LTKPreprocessor.h </p>
 */

class LTKPreprocessor : public LTKPreprocessorInterface
{
private:

     float m_sizeThreshold;                  //   threshold below which a trace will not be size normalized

     float m_loopThreshold;                  //   threshold below which a trace will not be considered a loop

     float m_aspectRatioThreshold;      //   threshold above which the aspect ratio of a trace is maintained after size normalization

     float m_dotThreshold;                   //   a threshold to detect shapes which are just dots

     float m_hookLengthThreshold1;

     float m_hookLengthThreshold2;

     float m_hookAngleThreshold;

     map<string, FN_PTR_PREPROCESSOR> m_preProcMap;    // Holds the address of preprocessor functions.

     bool m_preserveAspectRatio;

     bool m_preserveRelativeYPosition;

     LTKCaptureDevice m_captureDevice;

     LTKScreenContext m_screenContext;

     int m_filterLength;

     int m_traceDimension;

     int m_quantizationStep;                      //   quantization step for resampling

     string m_resamplingMethod;                   //  type of resampling scheme to be used point based or length based

     float m_interPointDist;                      //distance between successive points

public:
     /**
      * @name Constructors and Destructor
      */
     // @{


     LTKPreprocessor(const LTKControlInfo& controlInfo);

     /**
      * Copy Constructor
      */

    LTKPreprocessor(const LTKPreprocessor& preprocessor);

     /**
      * Destructor
      */

     virtual ~LTKPreprocessor();


     /**
      * @name Pre-processing Functions
      */
     // @{

     /**
      * This function normalizes the size for a trace group.
      * <p> If the width of the stroke set is more than the threshold,
      * they are normalised by width and scale; otherwise they are traslated to the
      * center if bounding box determined by scale. </p>
      *
      * @param traceGroup Object of LTKTraceGroup
      *
      * @return void
      */

     /**
      * @name Getter Functions
      */
     // @{


     /**
      * The function returns the scale(width or height) of the trace group assumed after normalization.
      * @param void
      *
      * @return The value of scale for coordinate axis.
      */


     bool getPreserveAspectRatio() const;

     /**
      * The function returns the scale(width or height) of the trace group assumed after normalization.
      * @param void
      *
      * @return The value of scale for coordinate axis.
      */

     bool getPreserveRealtiveYPosition() const;

     /**
      * The fucntion returns the size threshold that a trace group is required to have.
      * @param void
      *
      * @return The size threshold that is pre-determined.
      */

     float getSizeThreshold() const;

     /**
      * This function returns the loop threshold of a trace group.
      * @param void
      *
      * @return The value of loop threshold.
      */

     float getLoopThreshold() const;

     /**
      * This function returns the threshold below which aspect ratio will be maintained
      * @param void
      *
      * @return Threshold below which aspect ratio will be maintained
      */
     float getAspectRatioThreshold() const;

     /**
      * This function returns the threshold used to detect dots
      * @param void
      *
      * @return Threshold used to detect dots
      */
     float getDotThreshold() const;

     /**
      * This function returns the value of the m_quantizationStep variable of the preprocessor
      * @param void
      *
      * @return the m_quantizationStep of preprocessor
      */
     int getQuantizationStep() const;

     /**
      * This function returns the resampling method being used by the preprocessor
      * @param void
      *
      * @return the m_resamplingMethod value
      */
     string getResamplingMethod() const;

     /**
      * This function returns the value of the m_traceDimension variable of the preprocessor
      * @param void
      *
      * @return the m_traceDimension; of preprocessor
      */
     const int getTraceDimension() const;


     /**
      * This function returns the value of the m_filterLength variable of the preprocessor
      * @param void
      *
      * @return the m_filterLength of preprocessor
      */
     const int getFilterLength() const;

     /**
      * This function returns the max y coord of the tracegroup given to the findingBoundingBox function
      * @param void
      *
      * @return the max y coord of the tracegroup given to the findingBoundingBox function
      */

     const LTKCaptureDevice& getCaptureDevice() const;

     /**
      * This function returns the max y coord of the tracegroup given to the findingBoundingBox function
      * @param void
      *
      * @return the max y coord of the tracegroup given to the findingBoundingBox function
      */

     const LTKScreenContext& getScreenContext() const;

     /**
      * @name Setter Functions
      */
     // @{


     /**
      * This function sets member variable preserve aspect ratio with the flag specified.
      * @param flag can be true or false
      *
      * @return SUCCESS on successful set operation
      */

     void setPreserveAspectRatio(bool flag);
     /**
      * This function sets member variable preserve relative y position with the flag specified.
      * @param flag can be true or false
      *
      * @return SUCCESS on successful set operation
      */

     void setPreserveRelativeYPosition(bool flag);

     /**
      * This function sets the threshold value for a trace group.
      * @param sizeThreshold This threshold determines what normalization has to be done for a trace group.
      *
      * @return SUCCESS on successful set operation
      */

     int setSizeThreshold(float sizeThreshold);

     /**
      * This function sets the loop threshold if a trace group is curved.
      * @param loopThreshold Threshold to determine whether trace is a closed curve or not.
      *
      * @return SUCCESS on successful set operation
      */

     int setLoopThreshold(float loopThreshold);

     /**
      * This function sets threshold below which aspect ratio will be maintained after normalization
      * @param aspectRatioThreshold Threshold below which aspect ratio will be maintained
      *
      * @return SUCCESS on successful set operation
      */

     int setAspectRatioThreshold(float aspectRatioThreshold);

     /**
      * This function sets threshold to detect dots
      * @param dotThreshold Threshold to detect dots
      *
      * @return SUCCESS on successful set operation
      */

     int setDotThreshold(float dotThreshold);

     /**
      * This function sets filter length.
      * @param fileterLength
      *
      * @return SUCCESS on successful set operation
      */
     int setFilterLength (int filterLength);

     /**
      * This method sets the length threshold to detect hooks.
      *
      * @param hookLengthThreshold1 - length threshold to detect hooks.
      * @return SUCCESS on successful set operation
      */
     int setHookLengthThreshold1(float hookLengthThreshold1);

     /**
      * This method sets the length threshold to detect hooks
      *
      * @param hookLengthThreshold1 - length threshold to detect hooks.
      * @return SUCCESS on successful set operation
      */
     int setHookLengthThreshold2(float hookLengthThreshold2);

     /**
      * This method sets the angle threshold to detect hooks
      *
      * @param hookAngleThreshold - angle threshold to detect hooks
      * @return SUCCESS on successful set operation
      */
     int setHookAngleThreshold(float hookAngleThreshold);

    /**
      * This function returns the max y coord of the tracegroup given to the findingBoundingBox function
      * @param void
      *
      * @return the max y coord of the tracegroup given to the findingBoundingBox function
      */

     void setCaptureDevice(const LTKCaptureDevice& captureDevice);

     /**
      * This function returns the max y coord of the tracegroup given to the findingBoundingBox function
      * @param void
      *
      * @return the max y coord of the tracegroup given to the findingBoundingBox function
      */

     void setScreenContext(const LTKScreenContext& screenContext);


     /**
      * This function sets the value of the m_quantizationStep variable of the preprocessor
      * @param void
      *
      * @return SUCCESS on successful set operation
      */
     int setQuantizationStep(int quantizationStep) ;

     /**
      * This function sets the resampling method to be used by the preprocessor
      * @param void
      *
      * @return SUCCESS on successful set operation
      */
     int setResamplingMethod(const string& resamplingMethod);

     /**
      * This function adds the function name and the Address of the function into the
      * maping variable.
      * @param void
      *
      * @returns 0 on success.
      */

     void initFunAddrMap();

     /**
      * This function takes the function name as an argument and returns the
      * pointer to the function.
      * @param function name
      *
      * @return the pointer to the function.
      */

    FN_PTR_PREPROCESSOR getPreprocptr(const string &funName);

     /**
      * This function assigns the factory default values
      * for the preprocessor attributes
      * @param void
      *
      * @return 0 on success
      */

     void initPreprocFactoryDefaults();
     // @}

      /**
      * This function is called on a trace group to normalize it's size.
      *
      * @param inTraceGroup input trace group that must be normalized.
      * @param outTraceGroup The trace group whose traces are normalized.
      *
      * @return void
      *
      */

     int normalizeSize(const LTKTraceGroup& inTraceGroup, LTKTraceGroup& outTraceGroup);


     /**
      * This function is called on a trace group to normalize its orientation
      *
      * @param inTraceGroup input trace group that must be normalized.
      * @param outTraceGroup The trace group whose traces are normalized.
      *
      * @return void
      *
      */

     int normalizeOrientation(const LTKTraceGroup& inTraceGroup, LTKTraceGroup& outTraceGroup);

     /**
      * @param traceGroup Object of LTKTracegroup
      * @param traceDimension     Average number of points expected in a trace
      *
      * @return void
      */

     int duplicatePoints(const LTKTraceGroup& traceGroup, int traceDimension);

     /**
      * This function shifts the origin of eack stroke to the centroid of the stroke
      *
      * @param inTraceGroup  Trace group input, whose traces have to be centered.
      * @param outTraceGroup The output trace group that whose traces are centered.
      *
      * @return void
      */

     int centerTraces(const LTKTraceGroup& inTraceGroup, LTKTraceGroup& outTraceGroup);

     /**
      * This function dehooks the given tracegroup
      *
      * @param inTraceGroup Tracegroup which has to be dehooked
      * @param outTraceGroup The dehooked tracegroup
      *
      * @return void
      */

     int dehookTraces(const LTKTraceGroup& inTraceGroup, LTKTraceGroup& outTraceGroup);

     /**
      * This function removes consecutively repeating x, y coordinates (thinning)
      *
      * @param inTraceGroup trace group to be thinned
      * @param outTraceGroup thinned trace group
      *
      * @return SUCCESS on successful thinning operation
      */

     int removeDuplicatePoints(const LTKTraceGroup& inTraceGroup, LTKTraceGroup& outTraceGroup);

     /**
      * This function calculates the slopes of the strokes w.r.t maximum and minimum coordinates
      * and reorders the strokes.
      *
      * @param inTraceGroup Object of LTKTraceGroup, whose traces have to be ordered.
      * @param outTraceGroup The output trace group, whose traces are ordered.
      *
      * @return void
      */

     int orderTraces(const LTKTraceGroup& inTraceGroup, LTKTraceGroup& outTraceGroup);

     /**
      * This function reorients the given stroke
      *
      * @param inTrace Trace which has to be reversed
      * @param outTrace The reversed trace
      *
      * @return void
      */

     int reverseTrace(const LTKTrace& inTrace, LTKTrace& outTrace);

     /**
      * This function takes trace by trace from a trace group and resamples the points in them
      * according to the trace dimension.
      *
      * @param inTrace The input trace from trace group
      * @param resamplePoints The ideal number of points to which the trace must be resampled.
      * @param outTrace The output trace that has resampled points.
      *
      * @return void
      */

     int resampleTrace(const LTKTrace& inTrace,int resamplePoints, LTKTrace& outTrace);



     /**
      * This function smoothens the incoming tracegroup using moving average technique
      *
      * @param inTraceGroup The input trace group
      * @param filterLength is the number of points used for smoothening a single point
      * @param outTraceGrouo The output tracegroup that has smoothened points.
      *
      * @return SUCCESS
      */

     int smoothenTraceGroup(const LTKTraceGroup& inTraceGroup, LTKTraceGroup& outTraceGroup);

     /**
      * This method extracts the quantised Slope values from the input trace group.
      *
      * @param Input trace group, Quantised Slope Vector
      *
      * @return SUCCESS.
      */

     int getQuantisedSlope(const LTKTrace& trace, vector<int>& qSlopeVector);

     /**
      * This method identify the dominant points in the trace group based on the quantised
      * slope information.
      *
      * @param QSlopeVector,flexibility Index
      * @return Dominant Points
      */

    int determineDominantPoints(const vector<int>& qSlopeVector, int flexibilityIndex, vector<int>& dominantPts);

     /**
      * Computes the length of the given trace between two given point indices.
      *
      * @param trace - trace whose total/partial length is required.
      * @param fromPoint - point from which the trace length has to be computed.
      * @param toPoint - point to which the trace length has to be computed.
      *
      * @return total/partial length of the trace.
      */


    // May define TRACE_BEGIN_POINT TRACE_END_POINT macros and let fromPoint and toPoint
    //take these as default values
     int computeTraceLength(const LTKTrace& trace, int fromPoint, int toPoint, float& outLength);

     int resampleTraceGroup(const LTKTraceGroup& inTraceGroup,
                                 LTKTraceGroup& outTraceGroup
                                );

     int setTraceDimension(int traceDimension);

     // @}

private:

     /**
      * This function calculates slope of 2 points.
      *
      * @param x1,x2,y1,y2 coordinates of 2 points
      *
      * @return Slope of two points.
      */

     float calculateSlope(float x1,float y1,float x2,float y2);

     /**
      * This function calculates Euclidian distance between 2 points.
      *
      * @param x1,x2,y1,y2 Coordinates of two points.
      *
      * @return Euclidian Distance between two points.
      */

    float calculateEuclidDist(float x1,float x2,float y1,float y2);

     /**
      * This method calculates the swept angle of the curve.
      *
      * @param Input trace
      * @return Swept angle of the trace.
      */

    int calculateSweptAngle(const LTKTrace& trace, float& outSweptAngle);

     int readConfig(const string& cfgFilePath);

     bool isDot(const LTKTraceGroup& inTraceGroup);

};

#endif    //#ifndef __LTKPREPROCESSOR_H






















