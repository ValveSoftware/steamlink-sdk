/******************************************************************************
* Copyright (c) 2007 Hewlett-Packard Development Company, L.P.
* Permission is hereby granted, free of charge, to any person obtaining a copy 
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights 
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
* copies of the Software, and to permit persons to whom the Software is 
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies or 
* substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
* SOFTWARE
******************************************************************************/
/************************************************************************
 * SVN MACROS
 *
 * $LastChangedDate: 2008-07-10 15:23:21 +0530 (Thu, 10 Jul 2008) $
 * $Revision: 556 $
 * $Author: sharmnid $
 *
 ************************************************************************/
#ifndef __LTKPREPROCESSORINTERFACE_H
#define __LTKPREPROCESSORINTERFACE_H

#include "LTKTraceGroup.h"

class LTKPreprocessorInterface;

typedef int (LTKPreprocessorInterface::*FN_PTR_PREPROCESSOR)(const LTKTraceGroup& inTraceGroup, LTKTraceGroup& outTraceGroup);

class LTKPreprocessorInterface
{

public:

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
	 * The function get the function name as the argument and return the address of the
	 * function.
	 * @param funName
	 *
	 * @return address of the function.
	 */

    virtual FN_PTR_PREPROCESSOR getPreprocptr(const string &funName) = 0;

	/**
	 * @name Setter Functions
	 */
	// @{


	virtual int normalizeSize(const LTKTraceGroup& inTraceGroup, LTKTraceGroup& outTraceGroup) = 0;

	/**
	 * This function is called on a trace group to normalize its orientation
	 *
	 * @param inTraceGroup input trace group that must be normalized.
	 * @param outTraceGroup The trace group whose traces are normalized.
	 *
	 * @return void
	 *
	 */

	virtual int normalizeOrientation(const LTKTraceGroup& inTraceGroup, LTKTraceGroup& outTraceGroup) = 0;

	/**
	 * @param traceGroup Object of LTKTracegroup
	 * @param traceDimension     Average number of points expected in a trace
	 *
	 * @return void
	 */

	virtual int duplicatePoints(const LTKTraceGroup& traceGroup, int traceDimension) = 0;

	/**
	 * This function shifts the origin of eack stroke to the centroid of the stroke
	 *
	 * @param inTraceGroup  Trace group input, whose traces have to be centered.
	 * @param outTraceGroup The output trace group that whose traces are centered.
	 *
	 * @return void
	 */

	virtual int centerTraces(const LTKTraceGroup& inTraceGroup, LTKTraceGroup& outTraceGroup) = 0;

	/**
	 * This function dehooks the given tracegroup
	 *
	 * @param inTraceGroup Tracegroup which has to be dehooked
	 * @param outTraceGroup The dehooked tracegroup
	 *
	 * @return void
	 */

	virtual int dehookTraces(const LTKTraceGroup& inTraceGroup, LTKTraceGroup& outTraceGroup) = 0;

	/**
	 * This function removes consecutively repeating x, y coordinates (thinning)
	 *
	 * @param inTraceGroup trace group to be thinned
	 * @param outTraceGroup thinned trace group
	 *
	 * @return SUCCESS on successful thinning operation
	 */

	virtual int removeDuplicatePoints(const LTKTraceGroup& inTraceGroup, LTKTraceGroup& outTraceGroup) = 0;

	/**
	 * This function calculates the slopes of the strokes w.r.t maximum and minimum coordinates
	 * and reorders the strokes.
	 *
	 * @param inTraceGroup Object of LTKTraceGroup, whose traces have to be ordered.
	 * @param outTraceGroup The output trace group, whose traces are ordered.
	 *
	 * @return void
	 */

	virtual int orderTraces(const LTKTraceGroup& inTraceGroup, LTKTraceGroup& outTraceGroup) = 0;

	/**
	 * This function reorients the given stroke
	 *
	 * @param inTrace Trace which has to be reversed
	 * @param outTrace The reversed trace
	 *
	 * @return void
	 */

	virtual int reverseTrace(const LTKTrace& inTrace, LTKTrace& outTrace) = 0;

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

	virtual int resampleTrace(const LTKTrace& inTrace, int resamplePoint, LTKTrace& outTrace) = 0;


	/**
	 * This function smoothens the incoming tracegroup using moving average technique
	 *
	 * @param inTraceGroup The input trace group
	 * @param filterLength is the number of points used for smoothening a single point
	 * @param outTraceGrouo The output tracegroup that has smoothened points.
	 *
	 * @return SUCCESS
	 */

	virtual int smoothenTraceGroup(const LTKTraceGroup& inTraceGroup, LTKTraceGroup& outTraceGroup) = 0;

	virtual int resampleTraceGroup(const LTKTraceGroup& inTraceGroup,
											 LTKTraceGroup& outTraceGroup)=0;
	virtual int getQuantisedSlope(const LTKTrace& trace, vector<int>& qSlopeVector) = 0;

    virtual int determineDominantPoints(const vector<int>& qSlopeVector, int flexibilityIndex, vector<int>& dominantPts) = 0;

	virtual void setCaptureDevice(const LTKCaptureDevice& captureDevice) = 0;

	virtual void setScreenContext(const LTKScreenContext& screenContext) = 0;

	virtual bool getPreserveAspectRatio()const = 0 ;

	virtual float getAspectRatioThreshold()const= 0 ;

	virtual bool getPreserveRealtiveYPosition()const = 0 ;

	virtual float getSizeThreshold()const =0 ;

	virtual	float getDotThreshold()const = 0 ;

	virtual string getResamplingMethod()const = 0 ;

	virtual const int getTraceDimension()const = 0 ;

	virtual const int getFilterLength()const = 0 ;
		
	virtual ~LTKPreprocessorInterface(){};


};

#endif


