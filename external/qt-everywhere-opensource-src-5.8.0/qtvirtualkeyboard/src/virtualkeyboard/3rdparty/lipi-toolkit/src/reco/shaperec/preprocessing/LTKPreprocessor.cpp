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
 * $LastChangedDate: 2011-02-08 11:00:11 +0530 (Tue, 08 Feb 2011) $
 * $Revision: 832 $
 * $Author: dineshm $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Implementation of LTKPreprocessor which is a library of standard pre-processing functions
 *
 * CONTENTS:
 *	LTKPreprocessor
 *	~LTKPreprocessor
 *	normalizeSize
 *	normalizeOrientation
 *	resampleTrace
 *	smoothenTraceGroup
 *	setFilterLength
 *	centerTraces
 *	dehookTraces
 *	orderTraces
 *	reverseTrace
 *	duplicatePoints
 *	getNormalizedSize
 *	getSizeThreshold
 *	getLoopThreshold
 *	getAspectRatioThreshold
 *	getDotThreshold
 *	setNormalizedSize
 *	setSizeThreshold
 *	setLoopThreshold
 *	setAspectRatioThreshold
 *	setDotThreshold
 *	setHookLengthThreshold1
 *	setHookLengthThreshold2
 *	setHookAngleThreshold
 *	calculateSlope
 *	calculateEuclidDist
 *	getQuantisedSlope
 *	determineDominantPoints
 *	computeTraceLength
 *	removeDuplicatePoints
 *	calculateSweptAngle
 *	initFunAddrMap
 *	getPreprocptr
 *  setPreProcAttributes
 *  initPreprocFactoryDefaults
 *
 * AUTHOR:     Balaji R.
 *
 * DATE:       December 23, 2004
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#include "LTKTypes.h"

#include "LTKPreprocessor.h"

#include "LTKStringUtil.h"

#include "LTKLoggerUtil.h"

#include "LTKPreprocDefaults.h"

#include "LTKTraceGroup.h"

#include "LTKShapeRecoConfig.h"

#include "LTKInkUtils.h"

#include "LTKTrace.h"

#include "LTKErrors.h"

#include "LTKErrorsList.h"

#include "LTKConfigFileReader.h"

#include "LTKException.h"

#include "LTKChannel.h"

#include "LTKClassifierDefaults.h"

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: LTKPreprocessor
* DESCRIPTION	: Copy Constructor
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

LTKPreprocessor:: LTKPreprocessor(const LTKPreprocessor& preprocessor)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Entered copy constructor of LTKPreprocessor" <<endl;

	initFunAddrMap();
	initPreprocFactoryDefaults();

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Exiting copy constructor of LTKPreprocessor" <<endl;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: ~LTKPreprocessor
* DESCRIPTION	: destructor
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

LTKPreprocessor:: ~LTKPreprocessor()
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Entered destructor of LTKPreprocessor" <<endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Exiting destructor of LTKPreprocessor" <<endl;
}
/**********************************************************************************
* AUTHOR		: Dinesh M
* DATE			: 13-Oct-2006
* NAME			: getCaptureDevice
* DESCRIPTION	: get the value of the member variable m_captureDevice
* ARGUMENTS		:
* RETURNS		: returns m_captureDevice
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

const LTKCaptureDevice& LTKPreprocessor::getCaptureDevice() const
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) <<"Entered getCaptureDevice" <<endl;

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) <<  "Exiting getCaptureDevice" <<endl;

	return m_captureDevice;

}
/**********************************************************************************
* AUTHOR		: Dinesh M
* DATE			: 13-Oct-2006
* NAME			: getScreenContext
* DESCRIPTION	: get the value of the member variable m_screenContext
* ARGUMENTS		:
* RETURNS		: returns m_screenContext
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
const LTKScreenContext& LTKPreprocessor::getScreenContext() const
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Entered getScreenContext" <<endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Exiting getScreenContext" <<endl;

	return m_screenContext;

}

/**********************************************************************************
* AUTHOR		: Dinesh M
* DATE			: 13-Oct-2006
* NAME			: setCaptureDevice
* DESCRIPTION	: sets the member variable m_captureDevice
* ARGUMENTS		:
*
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
void LTKPreprocessor::setCaptureDevice(const LTKCaptureDevice& captureDevice)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Entered setCaptureDevice" <<endl;

	m_captureDevice = captureDevice;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Exiting setCaptureDevice" <<endl;

}


/**********************************************************************************
* AUTHOR		: Dinesh M
* DATE			: 13-Oct-2006
* NAME			: setScreenContext
* DESCRIPTION	: sets the member variable m_screenContext
* ARGUMENTS		:
*
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

void LTKPreprocessor::setScreenContext(const LTKScreenContext& screenContext)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Entered setScreenContext" <<endl;

	m_screenContext = screenContext;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Exiting setScreenContext" <<endl;

}


/**********************************************************************************
* AUTHOR		: Dinesh M
* DATE			: 24-Aug-2006
* NAME			: normalizeSize
* DESCRIPTION	: normalizes the size of the incoming trace group
* ARGUMENTS		: inTraceGroup - incoming tracegroup which is to be size normalized
*				  outTraceGroup - size noramlized inTraceGroup
* RETURNS		: SUCCESS on successful size normalization
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

int LTKPreprocessor::normalizeSize(const LTKTraceGroup& inTraceGroup, LTKTraceGroup& outTraceGroup)
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Entered LTKPreprocessor::normalizeSize" <<endl;

	float xScale;						//	scale factor along the x direction

	float yScale;						//	scale factor along the y direction

	float aspectRatio;					//	aspect ratio of the trace group

	vector<LTKTrace> normalizedTracesVec;	// holds the normalized traces

	LTKTrace trace;							//	a trace of the trace group

	floatVector xVec;						//	x channel values of a trace

	floatVector yVec;						//	y channel values of a trace

	floatVector normalizedXVec;				//	normalized x channel values of a trace

	floatVector normalizedYVec;				//	normalized y channel values of a trace

	float scaleX, scaleY, offsetX, offsetY;

	float xMin,yMin,xMax,yMax;

    int errorCode;

	// getting the bounding box information of the input trace group

	if( (errorCode = inTraceGroup.getBoundingBox(xMin,yMin,xMax,yMax)) != SUCCESS)
    {
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
        <<"Error: LTKPreprocessor::normalizeSize"<<endl;
        LTKReturnError(errorCode);
	}

	outTraceGroup = inTraceGroup;


	//	width of the bounding box at scalefactor = 1

	xScale = ((float)fabs(xMax - xMin))/inTraceGroup.getXScaleFactor();

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "xScale = " <<  xScale <<endl;

	// height of the bounding box at scalefactor = 1

	yScale = ((float)fabs(yMax - yMin))/inTraceGroup.getYScaleFactor();

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "yScale = " <<  yScale <<endl;

	if(m_preserveAspectRatio)
	{
		if(yScale > xScale)
		{
			aspectRatio = (xScale > EPS) ? (yScale/xScale) : m_aspectRatioThreshold + EPS;
		}
		else
		{
			aspectRatio = (yScale > EPS) ? (xScale/yScale) : m_aspectRatioThreshold + EPS;
		}
		if(aspectRatio > m_aspectRatioThreshold)
		{
			if(yScale > xScale)
				xScale = yScale;
			else
				yScale = xScale;
		}
	}


	offsetY = 0.0f;
	if(m_preserveRelativeYPosition )
	{
		offsetY = (yMin+yMax)/2.0f;
	}

	if(xScale <= (m_dotThreshold * m_captureDevice.getXDPI()) && yScale <= (m_dotThreshold * m_captureDevice.getYDPI()))
	{

		offsetX = PREPROC_DEF_NORMALIZEDSIZE/2;
		offsetY += PREPROC_DEF_NORMALIZEDSIZE/2;

		outTraceGroup.emptyAllTraces();

		for(int traceIndex=0;traceIndex<inTraceGroup.getNumTraces();++traceIndex)
		{
			LTKTrace tempTrace;

			inTraceGroup.getTraceAt(traceIndex,tempTrace);

			vector<float> newXChannel(tempTrace.getNumberOfPoints(),offsetX);
			vector<float> newYChannel(tempTrace.getNumberOfPoints(),offsetY);

			tempTrace.reassignChannelValues(X_CHANNEL_NAME, newXChannel);
			tempTrace.reassignChannelValues(Y_CHANNEL_NAME, newYChannel);

			outTraceGroup.addTrace(tempTrace);

		}

		return SUCCESS;
	}


	// finding the final scale and offset values for the x channel
	if((!m_preserveAspectRatio )&&(xScale < (m_sizeThreshold*m_captureDevice.getXDPI())))
	{
		scaleX = 1.0f;
		offsetX = PREPROC_DEF_NORMALIZEDSIZE/2.0 ;
	}
	else
	{
		scaleX =  PREPROC_DEF_NORMALIZEDSIZE / xScale ;
		offsetX = 0.0;
	}

	// finding the final scale and offset values for the y channel



	if((!m_preserveAspectRatio )&&(yScale < (m_sizeThreshold*m_captureDevice.getYDPI())))
	{
		offsetY += PREPROC_DEF_NORMALIZEDSIZE/2;
		scaleY = 1.0f;
	}
	else
	{
		scaleY = PREPROC_DEF_NORMALIZEDSIZE / yScale ;
	}


    //scaling the copy of the inTraceGroup in outTraceGroup according to new scale factors
	//and translating xmin_ymin of the trace group bounding box to the point (offsetX,offsetY).
	//Even though absolute location has to be specified for translateToX and translateToY,
	//since offsetX and offsetY are computed with respect to origin, they serve as absolute values

	if( (errorCode = outTraceGroup.affineTransform(scaleX,scaleY,offsetX,offsetY,XMIN_YMIN)) != SUCCESS)
    {
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
        <<"Error: LTKPreprocessor::normalizeSize"<<endl;
        LTKReturnError(errorCode);
	}

   	return SUCCESS;
}


/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: normalizeOrientation
* DESCRIPTION	: normalizes the orientation of the incoming trace group
* ARGUMENTS		: inTraceGroup - incoming tracegroup which is to be orientation normalized
*				  outTraceGroup - orientation noramlized inTraceGroup
* RETURNS		: SUCCESS on successful orientation normalization
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

int LTKPreprocessor::normalizeOrientation(const LTKTraceGroup& inTraceGroup, LTKTraceGroup& outTraceGroup)
{
	int traceIndex;

	int numTraces;

	float bboxDiagonalLength;

	float initialXCoord, finalXCoord;

	float initialYCoord, finalYCoord;

	float deltaX;

	float deltaY;

	float sweptAngle;

	int errorCode;

	vector<LTKTrace> tracesVec;

	tracesVec = inTraceGroup.getAllTraces();

	numTraces = tracesVec.size();

	vector<string> channelNames;

	channelNames.push_back(X_CHANNEL_NAME);

	channelNames.push_back(Y_CHANNEL_NAME);

	floatVector maxValues, minValues;

	vector<LTKTrace> tempTraceVector;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "numTraces = " <<  numTraces <<endl;

	for(traceIndex=0; traceIndex < numTraces; ++traceIndex)
	{
		const LTKTrace& trace = tracesVec.at(traceIndex);

	    if( (errorCode = LTKInkUtils::computeChannelMaxMin(trace, channelNames, maxValues, minValues)) != SUCCESS)
        {
		    LOG(LTKLogger::LTK_LOGLEVEL_ERR)
            <<"Error: LTKPreprocessor::normalizeOrientation"<<endl;
            LTKReturnError(errorCode);
	    }

		bboxDiagonalLength = calculateEuclidDist(minValues[0], maxValues[0], minValues[1], maxValues[1]);

		minValues.clear();

		maxValues.clear();

		if(bboxDiagonalLength == 0)
		{
			tempTraceVector.push_back(trace);
			continue;
		}

		floatVector xVec, yVec;

		if( (errorCode = trace.getChannelValues(X_CHANNEL_NAME, xVec))!= SUCCESS)
        {
		    LOG(LTKLogger::LTK_LOGLEVEL_ERR)
            <<"Error: LTKPreprocessor::normalizeOrientation"<<endl;
            LTKReturnError(errorCode);
	    }


		if( (errorCode = trace.getChannelValues(Y_CHANNEL_NAME, yVec)) != SUCCESS)
        {
		    LOG(LTKLogger::LTK_LOGLEVEL_ERR)
            <<"Error: LTKPreprocessor::normalizeOrientation"<<endl;
            LTKReturnError(errorCode);
	    }

		if(xVec.size()==0)
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR)
                  <<"Error : "<< EEMPTY_VECTOR <<":"<< getErrorMessage(EEMPTY_VECTOR)
                  <<"LTKPreprocessor::normalizeOrientation" <<endl;

            LTKReturnError(EEMPTY_VECTOR);
        }

		if(yVec.size()==0)
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR)
                  <<"Error : "<< EEMPTY_VECTOR <<":"<< getErrorMessage(EEMPTY_VECTOR)
                  <<"LTKPreprocessor::normalizeOrientation" <<endl;

            LTKReturnError(EEMPTY_VECTOR);
        }


		initialXCoord = xVec[0];

		finalXCoord = xVec[xVec.size() - 1];

		initialYCoord = yVec[0];

		finalYCoord = yVec[yVec.size() - 1];

		deltaX = fabs((finalXCoord - initialXCoord) / bboxDiagonalLength);

		deltaY = fabs((finalYCoord - initialYCoord) / bboxDiagonalLength);

		if( (errorCode = calculateSweptAngle(trace, sweptAngle)) != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			<<"Error: LTKPreprocessor::normalizeOrientation"<<endl;
			LTKReturnError(errorCode);
		}

		if((deltaX > m_loopThreshold && deltaY < m_loopThreshold && finalXCoord < initialXCoord) ||
		   (deltaY > m_loopThreshold && deltaX < m_loopThreshold && finalYCoord < initialYCoord) ||
		   (deltaX > m_loopThreshold && deltaY > m_loopThreshold && finalYCoord < initialYCoord) )
		{

			LTKTrace revTrace;

			if( (errorCode = reverseTrace(trace, revTrace)) != SUCCESS)
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)
				<<"Error: LTKPreprocessor::normalizeOrientation"<<endl;
				LTKReturnError(errorCode);
			}

			tempTraceVector.push_back(revTrace);
		}
		else if(sweptAngle < 0.0)
		{
			LTKTrace revTrace;

			if( (errorCode = reverseTrace(trace, revTrace)) != SUCCESS)
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)
				<<"Error: LTKPreprocessor::normalizeOrientation"<<endl;
				LTKReturnError(errorCode);
			}

			tempTraceVector.push_back(revTrace);
		}
		else
		{
			tempTraceVector.push_back(trace);
		}
	}
	outTraceGroup.setAllTraces(tempTraceVector,
                                 inTraceGroup.getXScaleFactor(),
                                 inTraceGroup.getYScaleFactor());

	return SUCCESS;
}


/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: resampleTrace
* DESCRIPTION	: spatially resamples the given trace to a given number of points
* ARGUMENTS		: inTrace - trace to be resampled
*				  resamplePoints - number of points required after resampling
*				  outTrace - resampled trace
* RETURNS		: SUCCESS on successful resample operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

int LTKPreprocessor::resampleTrace(const LTKTrace& inTrace, int resamplePoints, LTKTrace& outTrace)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Entered LTKPreprocessor::resampleTrace" <<endl;

    int pointIndex;							//	a variable to loop over the points of a trace

	int currentPointIndex =0;				//	index of the current point

	float xSum = 0.0f;								//	sum of the x coordinate values

	float ySum = 0.0f;								//	sum of the y coordinate values

	int numTracePoints;						//	number of points in a trace

	int ptIndex = 0;						//	index of point in a trace

	float x;								//	an x coord value

	float y;								//	an y coord value

	float xDiff;							//	differenc in x direction

	float yDiff;							//	diference in y direction

	float xTemp;							//	temp x storage

	float yTemp;							//	temp y storage

	float unitLength = 0;					//	estimated length of each segment after resampling

	float pointDistance;

	float balanceDistance = 0;				//	distance between the last resampled point and
											//	the next raw sample point

	float measuredDistance;

	float m1,m2;

	int errorCode;

	floatVector xVec, yVec;

	floatVector resampledXVec;

	floatVector resampledYVec;

	floatVector distanceVec;

	numTracePoints = inTrace.getNumberOfPoints();

	if(numTracePoints==0)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)
              <<"Error : "<< EEMPTY_TRACE <<":"<< getErrorMessage(EEMPTY_TRACE)
              <<"LTKPreprocessor::resampleTrace" <<endl;

        LTKReturnError(EEMPTY_TRACE);
    }

	if( (errorCode = inTrace.getChannelValues(X_CHANNEL_NAME, xVec))!= SUCCESS)
    {
	    LOG(LTKLogger::LTK_LOGLEVEL_ERR)
        <<"Error: LTKPreprocessor::resampleTrace"<<endl;
        LTKReturnError(errorCode);
    }


	if( (errorCode = inTrace.getChannelValues(Y_CHANNEL_NAME, yVec)) != SUCCESS)
    {
	    LOG(LTKLogger::LTK_LOGLEVEL_ERR)
        <<"Error: LTKPreprocessor::resampleTrace"<<endl;
        LTKReturnError(errorCode);
    }

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "resamplePoints = " <<  resamplePoints <<endl;

	if(resamplePoints < 1)
	{
		resamplePoints = 1;
	}

	if(resamplePoints == 1)
	{
        xSum = accumulate(xVec.begin(), xVec.end(), 0.0f);
		ySum = accumulate(yVec.begin(), yVec.end(), 0.0f);

		x = xSum/ numTracePoints; //As only 1 point is required, this ratio is computed.

		y = ySum/ numTracePoints;

		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "x mean = " <<  x <<endl;

		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "y mean = " <<  y <<endl;

		resampledXVec.push_back(x);

		resampledYVec.push_back(y);

		float2DVector allChannelValuesVec;
		allChannelValuesVec.push_back(resampledXVec);
		allChannelValuesVec.push_back(resampledYVec);

		if( (errorCode = outTrace.setAllChannelValues(allChannelValuesVec)) != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			<<"Error: LTKPreprocessor::resampleTrace"<<endl;
			LTKReturnError(errorCode);
		}

		/*if( (errorCode = outTrace.setChannelValues(X_CHANNEL_NAME, resampledXVec)) != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			<<"Error: LTKPreprocessor::resampleTrace"<<endl;
			LTKReturnError(errorCode);
		}
		if( (errorCode = outTrace.setChannelValues(Y_CHANNEL_NAME, resampledYVec)) != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			<<"Error: LTKPreprocessor::resampleTrace"<<endl;
			LTKReturnError(errorCode);
		}*/

	}
	else if(numTracePoints <= 1)
	{
		x = xVec.at(0);

		y = yVec.at(0);

		for(pointIndex = 0; pointIndex < resamplePoints; ++pointIndex)
		{
			resampledXVec.push_back(x);

			resampledYVec.push_back(y);

			LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "resampled point " << x << y <<endl;
		}

		float2DVector allChannelValuesVec;
		allChannelValuesVec.push_back(resampledXVec);
		allChannelValuesVec.push_back(resampledYVec);

		if( (errorCode = outTrace.setAllChannelValues(allChannelValuesVec)) != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			<<"Error: LTKPreprocessor::resampleTrace"<<endl;
			LTKReturnError(errorCode);
		}

		/*if( (errorCode = outTrace.setChannelValues(X_CHANNEL_NAME, resampledXVec)) != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			<<"Error: LTKPreprocessor::resampleTrace"<<endl;
			LTKReturnError(errorCode);
		}
		if( (errorCode = outTrace.setChannelValues(Y_CHANNEL_NAME, resampledYVec)) != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			<<"Error: LTKPreprocessor::resampleTrace"<<endl;
			LTKReturnError(errorCode);
		}*/
	}
	else
	{
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Point distances" <<endl;

		for( pointIndex = 0; pointIndex < (numTracePoints-1); ++pointIndex)
		{
			xDiff = xVec.at(pointIndex) - xVec.at(pointIndex+1);

			yDiff = yVec.at(pointIndex) - yVec.at(pointIndex+1);

			pointDistance = (float) (sqrt(xDiff*xDiff + yDiff*yDiff)); //distance between 2 points.

			unitLength += pointDistance; // finding the length of trace.

			distanceVec.push_back(pointDistance); //storing distances between every 2 consecutive points.

			LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "point distance: " <<  pointDistance <<endl;
		}

		unitLength /= (resamplePoints -1);

		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "unitLength = " <<  unitLength <<endl;

		x = xVec.at(0); // adding x of first point;

		y = yVec.at(0); // adding y of first point;

		resampledXVec.push_back(x);

		resampledYVec.push_back(y);

		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<    "resampled point " <<  x <<  y <<endl;

		for(pointIndex = 1; pointIndex < (resamplePoints-1); ++pointIndex)
		{
			measuredDistance = balanceDistance;

			while(measuredDistance < unitLength)
			{
				measuredDistance += distanceVec.at(ptIndex++);

				if(ptIndex == 1)
				{
					currentPointIndex = 1;
				}
				else
				{
					currentPointIndex++;
				}
			}

			if(ptIndex < 1) ptIndex = 1;

			m2 = measuredDistance - unitLength;

			m1 = distanceVec.at(ptIndex -1) - m2;

			if( fabs(m1+m2) > EPS)
			{
				xTemp =  (m1* xVec.at(currentPointIndex) + m2* xVec.at(currentPointIndex -1))/(m1+m2);

				yTemp =  (m1* yVec.at(currentPointIndex) + m2* yVec.at(currentPointIndex -1))/(m1+m2);
			}
			else
			{
				xTemp = xVec.at(currentPointIndex);

				yTemp = yVec.at(currentPointIndex);
			}

			resampledXVec.push_back(xTemp);

			resampledYVec.push_back(yTemp);

			LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "resampled point " << xTemp << yTemp  <<endl;

			balanceDistance = m2;
		}

		x = xVec.at(xVec.size() - 1); // adding x of last point;

		y = yVec.at(yVec.size() - 1); // adding y of last point;

		resampledXVec.push_back(x);

		resampledYVec.push_back(y);

		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<    "resampled point " <<  x <<  y  <<endl;

		float2DVector allChannelValuesVec;
		allChannelValuesVec.push_back(resampledXVec);
		allChannelValuesVec.push_back(resampledYVec);

		if( (errorCode = outTrace.setAllChannelValues(allChannelValuesVec)) != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			<<"Error: LTKPreprocessor::resampleTrace"<<endl;
			LTKReturnError(errorCode);
		}

		/*if( (errorCode = outTrace.setChannelValues(X_CHANNEL_NAME, resampledXVec)) != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			<<"Error: LTKPreprocessor::resampleTrace"<<endl;
			LTKReturnError(errorCode);
		}
		if( (errorCode = outTrace.setChannelValues(Y_CHANNEL_NAME, resampledYVec)) != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			<<"Error: LTKPreprocessor::resampleTrace"<<endl;
			LTKReturnError(errorCode);
		}*/

	}

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Exiting LTKPreprocessor::resampleTrace" <<endl;

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Mehul Patel
* DATE			: 04-APR-2005
* NAME			: smoothenTraceGroup
* DESCRIPTION	: smoothens the given tracegroup using moving average
* ARGUMENTS		: inTraceGroup - tracegroup to be smoothened
*				  m_filterLength - configurable parameter used for filtering
*				  outTraceGroup - smoothened TraceGroup
* RETURNS		: SUCCESS on successful smoothening operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
int LTKPreprocessor::smoothenTraceGroup(const LTKTraceGroup& inTraceGroup,
                                              LTKTraceGroup& outTraceGroup)
{
	int errorCode;
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Entered LTKPreprocessor::smoothenTrace" << endl;

	int loopIndex;				// Index used for looping

	int pointIndex;				// Index over all the points in the trace

	int traceIndex;				// Index over all the traces of a tracegroup

	int numTraces = inTraceGroup.getNumTraces();

	int actualIndex;         //

	float sumX, sumY;          //partial sum

    vector<LTKTrace> tempTraceVector;


	for(traceIndex = 0; traceIndex < numTraces; ++traceIndex)
	{

		LTKTrace inTrace;
		inTraceGroup.getTraceAt(traceIndex, inTrace);

		int numPoints = inTrace.getNumberOfPoints();			// total number of points in the trace

		floatVector newXChannel, newYChannel;			// temp vector to store the channelValues of outTrace

		//Retrieving the channels

		floatVector xChannel, yChannel;

		if( (errorCode = inTrace.getChannelValues(X_CHANNEL_NAME, xChannel))!= SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			<<"Error: LTKPreprocessor::smoothenTraceGroup"<<endl;
			LTKReturnError(errorCode);
		}


		if( (errorCode = inTrace.getChannelValues(Y_CHANNEL_NAME, yChannel)) != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			<<"Error: LTKPreprocessor::smoothenTraceGroup"<<endl;
			LTKReturnError(errorCode);
		}

		//iterating through the points

		for(pointIndex = 0; pointIndex < numPoints ; ++pointIndex)
		{
			sumX = sumY = 0;

			for(loopIndex = 0; loopIndex < m_filterLength; ++loopIndex)
			{
				actualIndex = (pointIndex-loopIndex);

				//checking for bounding conditions
				if (actualIndex <0 )
				{
					actualIndex = 0;
				}
				else if (actualIndex >= numPoints )
				{
					actualIndex = numPoints -1;
				}

				//accumulating values
				sumX +=  xChannel[actualIndex];
				sumY +=  yChannel[actualIndex];
			}

			//calculating average value
			sumX /= m_filterLength;
			sumY /= m_filterLength;


			//adding the values to the pre processed channels
			newXChannel.push_back(sumX);
			newYChannel.push_back(sumY);
		}

	/*outTrace.reassignChannelValues(X_CHANNEL_NAME, newXChannel);
	outTrace.reassignChannelValues(Y_CHANNEL_NAME, newYChannel);*/

		float2DVector allChannelValuesVec;
		allChannelValuesVec.push_back(newXChannel);
		allChannelValuesVec.push_back(newYChannel);

		LTKTrace outTrace;
		if( (errorCode = outTrace.setAllChannelValues(allChannelValuesVec)) != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			<<"Error: LTKPreprocessor::removeDuplicatePoints"<<endl;
			LTKReturnError(errorCode);
		}

		//adding the smoothed trace to the output trace group

        tempTraceVector.push_back(outTrace);
	} //traceIndex


	outTraceGroup.setAllTraces(tempTraceVector,
                                 inTraceGroup.getXScaleFactor(),
                                 inTraceGroup.getYScaleFactor());
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Exiting LTKPreprocessor::smoothenTrace" <<endl;

	return SUCCESS;
}


/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: centerTraces
* DESCRIPTION	: centers the traces of a trace group to the center of its bounding box
* ARGUMENTS		: inTraceGroup - trace group whose traces are to be centered
*				  outTraceGroup - trace group after its traces are centered
* RETURNS		: SUCCESS on successful centering operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

int LTKPreprocessor::centerTraces (const LTKTraceGroup& inTraceGroup,
                                     LTKTraceGroup& outTraceGroup)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Entered LTKPreprocessor::centerTraces" <<endl;

	int traceIndex;						//	a variable to loop over the traces of the trace group

	int pointIndex;						//	a variable to loop over the points of a trace

	int numTraces;						//	number of traces in the trace group

	int numTracePoints;					//	number of points in a trace

	LTKTrace trace;						//	a trace of the trace group

    vector<LTKTrace> centeredTracesVec;	//	centered traces of the trace group

	float xAvg,yAvg;					//	average x and y co-ordinate values

	floatVector xVec;					//	x coords of a trace

	floatVector yVec;					//	y coords of a trace

	int errorCode;

    vector<LTKTrace> tempTraceVector;

	/*	calculating the average x and y coordinate values and
	using them to center the traces of the trace group	*/

	numTraces = inTraceGroup.getNumTraces();

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
        "numTraces = " <<  numTraces <<endl;

	for(traceIndex=0; traceIndex< numTraces; traceIndex++)
	{
		inTraceGroup.getTraceAt(traceIndex, trace);

		numTracePoints = trace.getNumberOfPoints();

		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
            "numTracePoints = " <<  numTracePoints <<endl;

		if( (errorCode = trace.getChannelValues(X_CHANNEL_NAME, xVec))!= SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			<<"Error: LTKPreprocessor::centerTraces"<<endl;
			LTKReturnError(errorCode);
		}


		if( (errorCode = trace.getChannelValues(Y_CHANNEL_NAME, yVec)) != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			<<"Error: LTKPreprocessor::centerTraces"<<endl;
			LTKReturnError(errorCode);
		}

		for(pointIndex=0; pointIndex< numTracePoints; pointIndex++)
		{
			xAvg += xVec.at(pointIndex);

			yAvg += yVec.at(pointIndex);
		}

		xAvg /= numTracePoints;

		yAvg /= numTracePoints;

		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<< "xAvg = " <<  xAvg <<endl;

		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<< "yAvg = " <<  yAvg <<endl;

		for(pointIndex=0; pointIndex< numTracePoints; pointIndex++)
		{
			xVec.at(pointIndex) -= xAvg;

			yVec.at(pointIndex) -= yAvg;

			LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "centered point " <<  xVec.at(pointIndex) << yVec.at(pointIndex)  <<endl;
		}

		if( (errorCode = trace.reassignChannelValues(X_CHANNEL_NAME,xVec)) != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			<<"Error: LTKPreprocessor::centerTraces"<<endl;
			LTKReturnError(errorCode);
		}
		if( (errorCode = trace.reassignChannelValues(Y_CHANNEL_NAME,yVec)) != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			<<"Error: LTKPreprocessor::centerTraces"<<endl;
			LTKReturnError(errorCode);
		}

        tempTraceVector.push_back(trace);
	}

    outTraceGroup.setAllTraces(tempTraceVector,
                                 inTraceGroup.getXScaleFactor(),
                                 inTraceGroup.getYScaleFactor());

    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Exiting LTKPreprocessor::centerTraces" <<endl;

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: dehookTraces
* DESCRIPTION	: dehooks the traces of the tracegroup
* ARGUMENTS		:
* RETURNS		: SUCCESS on successful dehooking operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

int LTKPreprocessor::dehookTraces(const LTKTraceGroup& inTraceGroup,
                                      LTKTraceGroup& outTraceGroup)
{
	int  traceIndex;

	float numTraces;

	float dPX1, dPY1;

	float dPX2, dPY2;

	float dPX3, dPY3;

	float dPX4, dPY4;

	float L0;

	float angle;

	int firstPoint, lastPoint;

	int numDominantPoints;

	vector<int> quantizedSlopes;

	vector<int> dominantPoints;

	vector<string> channelNames;

	channelNames.push_back(X_CHANNEL_NAME);

	channelNames.push_back(Y_CHANNEL_NAME);

	floatVector maxValues, minValues;

	float scale;

	int errorCode;

	numTraces = inTraceGroup.getNumTraces();

    vector<LTKTrace> tempTraceVector;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "numTraces = " <<  numTraces <<endl;

	for(traceIndex=0; traceIndex< numTraces; traceIndex++)
	{
		LTKTrace trace;

		inTraceGroup.getTraceAt(traceIndex, trace);

		if( (errorCode = LTKInkUtils::computeChannelMaxMin(trace, channelNames, maxValues, minValues)) != SUCCESS)
        {
		    LOG(LTKLogger::LTK_LOGLEVEL_ERR)
            <<"Error: LTKPreprocessor::dehookTraces"<<endl;
            LTKReturnError(errorCode);
	    }

		if( (errorCode = computeTraceLength(trace, 0, trace.getNumberOfPoints() - 1,scale)) != SUCCESS)
        {
		    LOG(LTKLogger::LTK_LOGLEVEL_ERR)
            <<"Error: LTKPreprocessor::dehookTraces"<<endl;
            LTKReturnError(errorCode);
	    }

		floatVector xVec, yVec;

		if( (errorCode = trace.getChannelValues(X_CHANNEL_NAME, xVec))!= SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			<<"Error: LTKPreprocessor::dehookTraces"<<endl;
			LTKReturnError(errorCode);
		}

		if( (errorCode = trace.getChannelValues(Y_CHANNEL_NAME, yVec)) != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			<<"Error: LTKPreprocessor::dehookTraces"<<endl;
			LTKReturnError(errorCode);
		}

		if( (errorCode = getQuantisedSlope(trace, quantizedSlopes)) != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			<<"Error: LTKPreprocessor::dehookTraces"<<endl;
			LTKReturnError(errorCode);
		}

		if( (errorCode = determineDominantPoints(quantizedSlopes, 3, dominantPoints)) != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			<<"Error: LTKPreprocessor::dehookTraces"<<endl;
			LTKReturnError(errorCode);
		}

		numDominantPoints = dominantPoints.size();

		if(numDominantPoints > 2)
		{

			dPX1 = xVec[dominantPoints[0]];

			dPY1 = yVec[dominantPoints[0]];

			dPX2 = xVec[dominantPoints[1]];

			dPY2 = yVec[dominantPoints[1]];

			dPX3 = xVec[dominantPoints[1] + 1];

			dPY3 = yVec[dominantPoints[1] + 1];

			dPX4 = xVec[dominantPoints[1] - 1];

			dPY4 = yVec[dominantPoints[1] - 1];

			if( (errorCode = computeTraceLength(trace, dominantPoints[0], dominantPoints[1],L0))!= SUCCESS)
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)
				<<"Error: LTKPreprocessor::resampleTraceGroup"<<endl;
				LTKReturnError(errorCode);
			}

			angle = ( acos(((dPX1 - dPX2) * (dPX3 - dPX2) + (dPY1 - dPY2) * (dPY3 - dPY2)) /
		    (calculateEuclidDist(dPX2, dPX1, dPY2, dPY1) * calculateEuclidDist(dPX2, dPX3, dPY2, dPY3))) ) * 180 / 3.14;

			if((L0/scale) < this->m_hookLengthThreshold1 || (((L0/scale) < this->m_hookLengthThreshold2) &&
				(angle < this->m_hookAngleThreshold)))
			{
				firstPoint = dominantPoints[1];
			}
			else
			{
				firstPoint = dominantPoints[0];
			}

			dPX1 = xVec[dominantPoints[numDominantPoints - 1]];

			dPY1 = yVec[dominantPoints[numDominantPoints - 1]];

			dPX2 = xVec[dominantPoints[numDominantPoints - 2]];

			dPY2 = yVec[dominantPoints[numDominantPoints - 2]];

			dPX3 = xVec[dominantPoints[numDominantPoints - 2] - 1];

			dPY3 = yVec[dominantPoints[numDominantPoints - 2] - 1];

			dPX4 = xVec[dominantPoints[numDominantPoints - 2] + 1];

			dPY4 = yVec[dominantPoints[numDominantPoints - 2] + 1];

			if( (errorCode = computeTraceLength(trace, dominantPoints[numDominantPoints - 2], dominantPoints[numDominantPoints - 1],L0))!= SUCCESS)
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)
				<<"Error: LTKPreprocessor::resampleTraceGroup"<<endl;
				LTKReturnError(errorCode);
			}


			angle = (acos(((dPX1 - dPX2) * (dPX3 - dPX2) + (dPY1 - dPY2) * (dPY3 - dPY2)) /
		    (calculateEuclidDist(dPX2, dPX1, dPY2, dPY1) * calculateEuclidDist(dPX2, dPX3, dPY2, dPY3)))) * 180 / 3.14;

			if((L0/scale) < this->m_hookLengthThreshold1 || (((L0/scale) < this->m_hookLengthThreshold2) &&
				(angle < this->m_hookAngleThreshold)))
			{
				lastPoint = dominantPoints[numDominantPoints - 2];
			}
			else
			{
				lastPoint = dominantPoints[numDominantPoints - 1];
			}

			floatVector newXVec(xVec.begin() + firstPoint, xVec.begin() + lastPoint + 1);

			floatVector newYVec(yVec.begin() + firstPoint, yVec.begin() + lastPoint + 1);


			float2DVector allChannelValuesVec;
			allChannelValuesVec.push_back(newXVec);
			allChannelValuesVec.push_back(newYVec);

			LTKTrace newTrace;
			if( (errorCode = newTrace.setAllChannelValues(allChannelValuesVec)) != SUCCESS)
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)
				<<"Error: LTKPreprocessor::dehookTraces"<<endl;
				LTKReturnError(errorCode);
			}

            tempTraceVector.push_back(newTrace);

		}
		else
		{
		    tempTraceVector.push_back(trace);

            /*
			if( (errorCode = outTraceGroup.addTrace(trace)) != SUCCESS)
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)
				<<"Error: LTKPreprocessor::dehookTraces"<<endl;
				LTKReturnError(errorCode);
			}
			*/
		}

       outTraceGroup.setAllTraces(tempTraceVector,
                                 inTraceGroup.getXScaleFactor(),
                                 inTraceGroup.getYScaleFactor());

		quantizedSlopes.clear();

		dominantPoints.clear();

		maxValues.clear();

		minValues.clear();

	}

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: orderTraces
* DESCRIPTION	: reorders the traces to a pre-defined empirical order
* ARGUMENTS		:
* RETURNS		: SUCCESS on successful reordering operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

int LTKPreprocessor::orderTraces (const LTKTraceGroup& inTraceGroup,
                                    LTKTraceGroup& outTraceGroup)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Entered LTKPreprocessor::orderTraces" <<endl;

	//	yet to be implemented

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
	"Exiting LTKPreprocessor::orderTraces" <<endl;

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: reorientTraces
* DESCRIPTION	: reorients the traces to a pre-defined empirical direction
* ARGUMENTS		:
* RETURNS		: SUCCESS on successful reorientation operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

int LTKPreprocessor::reverseTrace(const LTKTrace& inTrace, LTKTrace& outTrace)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Entered LTKPreprocessor::reverseTrace" <<endl;

	int pointIndex;

	floatVector revXVec, revYVec;

	floatVector xVec, yVec;

	int errorCode;

	if( (errorCode = inTrace.getChannelValues(X_CHANNEL_NAME, xVec)) != SUCCESS)
    {
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
        <<"Error: LTKPreprocessor::reverseTrace"<<endl;
        LTKReturnError(errorCode);
	}


	if( (errorCode = inTrace.getChannelValues(Y_CHANNEL_NAME, yVec)) != SUCCESS)
    {
	    LOG(LTKLogger::LTK_LOGLEVEL_ERR)
        <<"Error: LTKPreprocessor::reverseTrace"<<endl;
        LTKReturnError(errorCode);
    }


	//	reversing the trace

	for(pointIndex = xVec.size() - 1; pointIndex >= 0 ; --pointIndex)
	{
		revXVec.push_back(xVec[pointIndex]);

		revYVec.push_back(yVec[pointIndex]);
	}


	outTrace = inTrace;

	outTrace.reassignChannelValues(X_CHANNEL_NAME, revXVec);
	outTrace.reassignChannelValues(Y_CHANNEL_NAME, revYVec);



	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Exiting LTKPreprocessor::reverseTrace" <<endl;

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: duplicatePoints
* DESCRIPTION	:
* ARGUMENTS		:
* RETURNS		: SUCCESS on successful duplication of points operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

int LTKPreprocessor::duplicatePoints (const LTKTraceGroup& traceGroup, int traceDimension)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Entered LTKPreprocessor::duplicatePoints" <<endl;

	//	yet to be implemented

	LOG( LTKLogger::LTK_LOGLEVEL_ERR)<<  "LTKPreprocessor::duplicatePoints - method not implemented" <<endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Exiting LTKPreprocessor::duplicatePoints" <<endl;

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Dinesh M
* DATE			: 24-Aug-2006
* NAME			: getPreserveAspectRatio
* DESCRIPTION	: gets the value of the m_preserveAspectRatio member variable
* ARGUMENTS		:
* RETURNS		: true or false
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

bool LTKPreprocessor::getPreserveAspectRatio() const
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Entered LTKPreprocessor::getPreserveAspectRatio" <<endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Exiting LTKPreprocessor::getPreserveAspectRatio with return value " <<  m_preserveAspectRatio <<endl;

	return m_preserveAspectRatio;
}

/**********************************************************************************
* AUTHOR		: Dinesh M
* DATE			: 24-Aug-2006
* NAME			: getPreserveRealtiveYPosition
* DESCRIPTION	: gets the value of the m_preserveAspectRatio member variable
* ARGUMENTS		:
* RETURNS		: true or false
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

bool LTKPreprocessor::getPreserveRealtiveYPosition() const
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Entered LTKPreprocessor::getPreserveRealtiveYPosition" <<endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Exiting LTKPreprocessor::getPreserveRealtiveYPosition with return value " <<  m_preserveRelativeYPosition <<endl;

	return m_preserveRelativeYPosition;
}
/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getSizeThreshold
* DESCRIPTION	: gets the size threshold below which traces will not be rescaled
* ARGUMENTS		:
* RETURNS		: size threshold below which traces will not be rescaled
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

float LTKPreprocessor::getSizeThreshold () const
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Entered LTKPreprocessor::getSizeThreshold" <<endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Exiting LTKPreprocessor::getSizeThreshold with return value " <<  m_sizeThreshold <<endl;

	return m_sizeThreshold;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getLoopThreshold
* DESCRIPTION	: gets the threshold below which the trace would considered a loop
* ARGUMENTS		:
* RETURNS		: threshold below which the trace would considered a loop
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

float LTKPreprocessor::getLoopThreshold () const
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Entered LTKPreprocessor::getLoopThreshold" <<endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Exiting LTKPreprocessor::getLoopThreshold with return value " <<  m_loopThreshold <<endl;

	return m_loopThreshold;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getAspectRatioThreshold
* DESCRIPTION	: returns threshold below which aspect ration will be maintained
* ARGUMENTS		:
* RETURNS		: threshold below which aspect ration will be maintained
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

float LTKPreprocessor::getAspectRatioThreshold () const
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Entered LTKPreprocessor::getAspectRatioThreshold" <<endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Exiting LTKPreprocessor::getAspectRatioThreshold with return value " <<  m_aspectRatioThreshold <<endl;

	return m_aspectRatioThreshold;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getDotThreshold
* DESCRIPTION	: returns threshold to detect dots
* ARGUMENTS		:
* RETURNS		: threshold to detect dots
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

float LTKPreprocessor::getDotThreshold () const
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Entered LTKPreprocessor::getDotThreshold" <<endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Exiting LTKPreprocessor::getDotThreshold with return value " <<  m_dotThreshold <<endl;

	return m_dotThreshold;
}



/**********************************************************************************
* AUTHOR		: Dinesh M
* DATE			: 31-May-2007
* NAME			: getQuantizationStep
* DESCRIPTION	: returns the value of the quantization step used in resampling
* ARGUMENTS		:
* RETURNS		: return the m_quantizationStep of the preprocessor
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
int LTKPreprocessor::getQuantizationStep() const
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Entered LTKPreprocessor::getQuantizationStep" <<endl;

	return m_quantizationStep;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Exiting LTKPreprocessor::getQuantizationStep with return value " <<  m_quantizationStep <<endl;


}

/**********************************************************************************
* AUTHOR		: Dinesh M
* DATE			: 31-May-2007
* NAME			: getResamplingMethod
* DESCRIPTION	: returns the type of resampling method being used in preprocessing
* ARGUMENTS		:
* RETURNS		: the m_resamplingMethod value
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
string LTKPreprocessor::getResamplingMethod() const
{
	LOG( LTKLogger::LTK_LOGLEVEL_INFO)<<   "Entered LTKPreprocessor::getResamplingMethod" <<endl;

	return m_resamplingMethod;

    LOG( LTKLogger::LTK_LOGLEVEL_INFO)<<   "Exiting LTKPreprocessor::getResamplingMethod" <<endl;
}
/**********************************************************************************
* AUTHOR		: Srinivasa Vithal, Ch.
* DATE			: 18-Jun-2008
* NAME			: getTraceDimension
* DESCRIPTION	: returns the value of resamplingTraceDimension
* ARGUMENTS		:
* RETURNS		: the m_traceDimension value
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
const int LTKPreprocessor::getTraceDimension() const
{
	LOG( LTKLogger::LTK_LOGLEVEL_INFO)<<   "Entered LTKPreprocessor::getTraceDimension" <<endl;

	return m_traceDimension;

    LOG( LTKLogger::LTK_LOGLEVEL_INFO)<<   "Exiting LTKPreprocessor::getTraceDimension" <<endl;
}

/**********************************************************************************
* AUTHOR		: Srinivasa Vithal, Ch.
* DATE			: 18-Jun-2008
* NAME			: getFilterLength
* DESCRIPTION	: returns the value of m_filterLength
* ARGUMENTS		:
* RETURNS		: the m_filterLength value
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
const int LTKPreprocessor::getFilterLength() const
{
	LOG( LTKLogger::LTK_LOGLEVEL_INFO)<<   "Entered LTKPreprocessor::getFilterLength" <<endl;

	return m_filterLength;

    LOG( LTKLogger::LTK_LOGLEVEL_INFO)<<   "Exiting LTKPreprocessor::getFilterLength" <<endl;
}


/**********************************************************************************
* AUTHOR		: Dinesh M
* DATE			: 24-AUG-2006
* NAME			: setPreserveAspectRatio
* DESCRIPTION	: sets the m_preserveAspectRatio member variable to the given flag
* ARGUMENTS		: flag - true or false
* RETURNS		: SUCCESS on successful set operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

void LTKPreprocessor::setPreserveAspectRatio(bool flag)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Entered LTKPreprocessor::setPreserveAspectRatio" <<endl;

	this->m_preserveAspectRatio = flag;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "m_preserveAspectRatio = " <<  m_preserveAspectRatio <<endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Exiting LTKPreprocessor::setPreserveAspectRatio" <<endl;

}

/**********************************************************************************
* AUTHOR		: Dinesh M
* DATE			: 24-AUG-2006
* NAME			: setPreserveRelativeYPosition
* DESCRIPTION	: sets the m_preserveRelativeYPosition member variable to the given flag
* ARGUMENTS		: flag - true or false
* RETURNS		: SUCCESS on successful set operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

void LTKPreprocessor::setPreserveRelativeYPosition(bool flag)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Entered LTKPreprocessor::setPreserveRelativeYPosition" <<endl;

	this->m_preserveRelativeYPosition = flag;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "m_preserveRelativeYPosition = " <<  m_preserveRelativeYPosition <<endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Exiting LTKPreprocessor::setPreserveRelativeYPosition" <<endl;

}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: setSizeThreshold
* DESCRIPTION	: sets the size threshold below which traces will not be rescaled
* ARGUMENTS		: sizeThreshold - size threshold below which traces will not be rescaled
* RETURNS		: SUCCESS on successful set operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* Nidhi Sharma      13-Sept-2007        Added check on input parameter
*************************************************************************************/

int LTKPreprocessor::setSizeThreshold (float sizeThreshold)
{
   	if(sizeThreshold < 0)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)
              <<"Error : "<< ENEGATIVE_NUM <<":"<< getErrorMessage(ENEGATIVE_NUM)
              <<"LTKPreprocessor::setNormalizedSize" <<endl;

        LTKReturnError(ENEGATIVE_NUM);
    }

	if(sizeThreshold > PREPROC_DEF_NORMALIZEDSIZE/100)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)
              <<"Error : "<< ECONFIG_FILE_RANGE <<":"<< getErrorMessage(ECONFIG_FILE_RANGE)
              <<"LTKPreprocessor::setNormalizedSize" <<endl;

        LTKReturnError(ECONFIG_FILE_RANGE);
    }

	this->m_sizeThreshold = sizeThreshold;

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: setLoopThreshold
* DESCRIPTION	: sets the threshold below which the trace would considered a loop
* ARGUMENTS		: loopThreshold - the threshold below which the trace would considered a loop
* RETURNS		: SUCCESS on successful set operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

int LTKPreprocessor::setLoopThreshold (float loopThreshold)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Entered LTKPreprocessor::setLoopThreshold" <<endl;

	this->m_loopThreshold = loopThreshold;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "m_loopThreshold = " <<  m_loopThreshold <<endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Exiting LTKPreprocessor::setLoopThreshold" <<endl;

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: setAspectRatioThreshold
* DESCRIPTION	: sets the threshold below which aspect ratio will be maintained
* ARGUMENTS		: aspectRatioThreshold - threshold below which aspect ratio will be maintained
* RETURNS		: SUCCESS on successful set operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* Nidhi Sharma      13-Sept-2007        Added check on input parameter
*************************************************************************************/

int LTKPreprocessor::setAspectRatioThreshold (float aspectRatioThreshold)
{
    if(aspectRatioThreshold < 1)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)
              <<"Error : "<< ENON_POSITIVE_NUM <<":"<< getErrorMessage(ENON_POSITIVE_NUM)
              <<"LTKPreprocessor::setAspectRatioThreshold" <<endl;

        LTKReturnError(ENON_POSITIVE_NUM);
    }

	this->m_aspectRatioThreshold = aspectRatioThreshold;

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: setDotThreshold
* DESCRIPTION	: sets the threshold to detect dots
* ARGUMENTS		: dotThreshold - threshold to detect dots
* RETURNS		: SUCCESS on successful set operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* Nidhi Sharma      13-Sept-2007        Added a check on input parameter
*************************************************************************************/

int LTKPreprocessor::setDotThreshold (float dotThreshold)
{
	if(dotThreshold <= 0)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)
              <<"Error : "<< ENON_POSITIVE_NUM <<":"<< getErrorMessage(ENON_POSITIVE_NUM)
              <<"LTKPreprocessor::setDotThreshold" <<endl;

        LTKReturnError(ENON_POSITIVE_NUM);
    }
    this->m_dotThreshold = dotThreshold;

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Vijayakumara M.
* DATE			: 25-OCT-2005
* NAME			: setFilterLength
* DESCRIPTION	: sets the fileter length
* ARGUMENTS		: filterLength - filter Length
* RETURNS		: SUCCESS on successful set operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
int LTKPreprocessor::setFilterLength (int filterLength)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Entered LTKPreprocessor::setFilterLength" <<endl;

	if(filterLength <= 0)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)
              <<"Error : "<< ENON_POSITIVE_NUM <<":"<< getErrorMessage(ENON_POSITIVE_NUM)
              <<"LTKPreprocessor::setFilterLength" <<endl;

        LTKReturnError(ENON_POSITIVE_NUM);
    }
	this->m_filterLength = filterLength;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "m_filterLength = " + m_filterLength <<endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Exiting LTKPreprocessor::setFilterLength" <<endl;

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: setHookLengthThreshold1
* DESCRIPTION	: sets the length threshold to detect hooks
* ARGUMENTS		: hookLengthThreshold1 - length threshold to detect hooks
* RETURNS		: SUCCESS on successful set operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
int LTKPreprocessor::setHookLengthThreshold1(float hookLengthThreshold1)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Entered LTKPreprocessor::setHookLengthThreshold1" <<endl;

	if(hookLengthThreshold1 < 0)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)
              <<"Error : "<< ENEGATIVE_NUM <<":"<< getErrorMessage(ENEGATIVE_NUM)
              <<"LTKPreprocessor::setHookLengthThreshold1" <<endl;

        LTKReturnError(ENEGATIVE_NUM);
    }

	this->m_hookLengthThreshold1 = hookLengthThreshold1;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "m_hookLengthThreshold1 = " <<  m_hookLengthThreshold1 <<endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Exiting LTKPreprocessor::setHookLengthThreshold1" <<endl;

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: setHookLengthThreshold2
* DESCRIPTION	: sets the length threshold to detect hooks
* ARGUMENTS		: hookLengthThreshold2 - length threshold to detect hooks
* RETURNS		: SUCCESS on successful set operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

int LTKPreprocessor::setHookLengthThreshold2(float hookLengthThreshold2)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Entered LTKPreprocessor::setHookLengthThreshold2" <<endl;
	if(hookLengthThreshold2 < 0)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)
              <<"Error : "<< ENEGATIVE_NUM <<":"<< getErrorMessage(ENEGATIVE_NUM)
              <<"LTKPreprocessor::setHookLengthThreshold2" <<endl;

        LTKReturnError(ENEGATIVE_NUM);
    }

	this->m_hookLengthThreshold2 = hookLengthThreshold2;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "m_hookLengthThreshold2 = " <<  m_hookLengthThreshold2 <<endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Exiting LTKPreprocessor::setHookLengthThreshold2" <<endl;

	return SUCCESS;

}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: setHookAngleThreshold
* DESCRIPTION	: sets the angle threshold to detect hooks
* ARGUMENTS		: hookAngleThreshold - angle threshold to detect hooks
* RETURNS		: SUCCESS on successful set operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

int LTKPreprocessor::setHookAngleThreshold(float hookAngleThreshold)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Entered LTKPreprocessor::setHookAngleThreshold" <<endl;

	if(hookAngleThreshold < 0)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)
              <<"Error : "<< ENEGATIVE_NUM <<":"<< getErrorMessage(ENEGATIVE_NUM)
              <<"LTKPreprocessor::setHookAngleThreshold" <<endl;

        LTKReturnError(ENEGATIVE_NUM);
    }

	this->m_hookAngleThreshold = hookAngleThreshold;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "m_hookAngleThreshold = " <<  m_hookAngleThreshold <<endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Exiting LTKPreprocessor::setHookAngleThreshold" <<endl;

	return SUCCESS;

}

/**********************************************************************************
* AUTHOR		: Dinesh M
* DATE			: 31-May-2007
* NAME			: setQuantizationStep
* DESCRIPTION	: sets the value of the quantization step used in resampling
* ARGUMENTS		:
* RETURNS		: SUCCESS on successful set operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*Nidhi Sharma       13-Sept-2007        Added a check on input parameter
*************************************************************************************/


int LTKPreprocessor::setQuantizationStep(int quantizationStep)
{
   	if(quantizationStep < 1)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)
              <<"Error : "<< ECONFIG_FILE_RANGE <<":"<< getErrorMessage(ECONFIG_FILE_RANGE)
              <<"LTKPreprocessor::setQuantizationStep" <<endl;

        LTKReturnError(ECONFIG_FILE_RANGE);
    }

	m_quantizationStep = quantizationStep;

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Dinesh M
* DATE			: 31-May-2007
* NAME			: setResamplingMethod
* DESCRIPTION	: sets the type of resampling method being used in preprocessing
* ARGUMENTS		:
* RETURNS		: SUCCESS on successful set operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* Nidhi Sharma      13-Sept-2007        Added a check on input parameter
*************************************************************************************/
int LTKPreprocessor::setResamplingMethod(const string& resamplingMethod)
{
    int returnVal = FAILURE;

    if ( LTKSTRCMP(resamplingMethod.c_str(), LENGTHBASED) == 0 ||
         LTKSTRCMP(resamplingMethod.c_str(), POINTBASED ) == 0 ||
		 LTKSTRCMP(resamplingMethod.c_str(), INTERPOINTDISTBASED) == 0)
    {
        m_resamplingMethod = resamplingMethod;

        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Resampling Method = " << m_resamplingMethod << endl;

        return SUCCESS;
    }

    LOG(LTKLogger::LTK_LOGLEVEL_ERR)
        <<"Error : "<< ECONFIG_FILE_RANGE <<":"<< getErrorMessage(ECONFIG_FILE_RANGE)
        <<"LTKPreprocessor::setResamplingMethod" <<endl;

    LTKReturnError(ECONFIG_FILE_RANGE);
}



/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: calculateSlope
* DESCRIPTION	: calculates the slope of the line joining two 2-d points
* ARGUMENTS		: x1 - x co-ordinate of point 1
*				  y1 - y co-ordinate of point 1
*				  x2 - x co-ordinate of point 2
*				  y2 - y co-ordinate of point 2
* RETURNS		: slope of the line joining the two 2-d points
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

float LTKPreprocessor::calculateSlope (float x1,float y1,float x2,float y2)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Entered LTKPreprocessor::calculateSlope" <<endl;

	//	yet to be implemented

	float slope=0.0f;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Exiting LTKPreprocessor::calculateSlope" <<endl;

	return slope;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: calculateEuclidDist
* DESCRIPTION	: calculates the euclidean distance between two 2-d points
* ARGUMENTS		: x1 - x co-ordinate of point 1
*				  x2 - x co-ordinate of point 2
*				  y1 - y co-ordinate of point 1
*				  y2 - y co-ordinate of point 2
* RETURNS		: euclidean distance between the two 2-d points
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

float LTKPreprocessor::calculateEuclidDist(float x1,float x2,float y1,float y2)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Entered LTKPreprocessor::calculateEuclidDist" <<endl;

	float euclidDistance;

	euclidDistance = sqrt(((x2 - x1) * (x2 - x1)) + ((y2 - y1) * (y2 - y1)));

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Exiting LTKPreprocessor::calculateEuclidDist" <<endl;

	return euclidDistance;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 19-MAR-2005
* NAME			: GetQuantisedSlope
* DESCRIPTION	: Extracts the quantised Slope values from the input trace group.
* ARGUMENTS		: Input trace group
* RETURNS		: Quantised Slope Vector
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

int LTKPreprocessor::getQuantisedSlope(const LTKTrace& trace, vector<int>& qSlopeVector)
{
	int errorCode;
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Entered DTWFeatureExtractor::GetQuantisedSlope" <<endl;

	floatVector slope;		    //Is a temporary variable which stores the slope values computed at each coordinate.

	int dimension;				//Stores the dimension of the CoordinatePoints

	float dx,dy;				//Temporary variales.

	int pointIndex;

	floatVector xVec, yVec;

	if( (errorCode = trace.getChannelValues(X_CHANNEL_NAME, xVec))!= SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
		<<"Error: LTKPreprocessor::centerTraces"<<endl;
		LTKReturnError(errorCode);
	}

	if( (errorCode = trace.getChannelValues(Y_CHANNEL_NAME, yVec)) != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
		<<"Error: LTKPreprocessor::centerTraces"<<endl;
		LTKReturnError(errorCode);
	}

	qSlopeVector.clear();

	dimension = trace.getNumberOfPoints(); //character.size();

	for(pointIndex=0; pointIndex < dimension-1; ++pointIndex)
	{
		dx = xVec[pointIndex+1] - xVec[pointIndex];

		dy = yVec[pointIndex+1] - yVec[pointIndex];

		if(fabs(dx) < EPS && fabs(dy) < EPS)
		{
			slope.push_back(1000.0);
		}
		else if(fabs(dx) < EPS)
		{
			if(dy > 0.0)
			{
				slope.push_back(90.0);
			}
			else
			{
				slope.push_back(-90.0);
			}
		}
		else
		{
			slope.push_back((float)atan((double)(dy/dx))*(180/3.14));
		}
	}

	slope.push_back(1000.0);

	for(pointIndex = 0; pointIndex < dimension - 1; ++pointIndex)
	{
		if(slope[pointIndex] == 1000.0)
		{
			qSlopeVector.push_back(-1);
		}
		else if((xVec[pointIndex+1] >= xVec[pointIndex]) && (slope[pointIndex]< 22.5) && (slope[pointIndex]>= -22.5))
		{
			qSlopeVector.push_back(1);
		}

		else if((xVec[pointIndex+1] >= xVec[pointIndex]) && (yVec[pointIndex+1] >= yVec[pointIndex]) && (slope[pointIndex] < 67.5) && (slope[pointIndex]>=22.5))
		{
			qSlopeVector.push_back(2);
		}

		else if((yVec[pointIndex+1] >= yVec[pointIndex]) && ((slope[pointIndex]>= 67.5) || (slope[pointIndex] < -67.5)) )
		{
			qSlopeVector.push_back(3);
		}

		else if((xVec[pointIndex+1] < xVec[pointIndex]) && (yVec[pointIndex+1] >= yVec[pointIndex]) && (slope[pointIndex]< -22.5 ) && (slope[pointIndex]>= -67.5))
		{
			qSlopeVector.push_back(4);
		}

		else if((xVec[pointIndex+1] < xVec[pointIndex]) && (slope[pointIndex] >= -22.5) && (slope[pointIndex] < 22.5))
		{
			qSlopeVector.push_back(5);
		}

		else if((xVec[pointIndex+1] < xVec[pointIndex]) && (yVec[pointIndex+1] < yVec[pointIndex]) && (slope[pointIndex]>=22.5) && (slope[pointIndex]< 67.5))
		{
			qSlopeVector.push_back(6);
		}

		else if((yVec[pointIndex+1] < yVec[pointIndex]) && ((slope[pointIndex]>= 67.5) || (slope[pointIndex]< -67.5)))
		{
			qSlopeVector.push_back(7);
		}

		else if((xVec[pointIndex+1] >= xVec[pointIndex]) && (yVec[pointIndex+1] < yVec[pointIndex]) && (slope[pointIndex]>= -67.5) && (slope[pointIndex]< -22.5))
		{
			qSlopeVector.push_back(8);
		}
	}

	if(dimension >= 2)
	{
		qSlopeVector.push_back(qSlopeVector[dimension-2]);
	}
	else
	{
		qSlopeVector.push_back(-1);
	}

	slope.clear();


	return SUCCESS;

}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 19-MAR-2005
* NAME			: DetermineDominantPoints
* DESCRIPTION	: Identify the dominant points in the trace group based on the quantised slope information
* ARGUMENTS		: QSlopeVector,flexibility Index
* RETURNS		: Dominant Points
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

int LTKPreprocessor::determineDominantPoints(const vector<int>& qSlopeVector, int flexibilityIndex, vector<int>& dominantPts)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Entered DTWFeatureExtractor::DetermineDominantPoints" <<endl;

	int initSlope;				//Temporary Variable to store the slope value at the previous point.

	dominantPts.clear();

	dominantPts.push_back(0);	//adding the first point

	initSlope = qSlopeVector[0];

	int pointIndex;

	for(pointIndex = 1; pointIndex < qSlopeVector.size() - 1; ++pointIndex)
	{
		if(initSlope == -1)
		{
			initSlope = qSlopeVector[pointIndex];

			continue;
		}

		if(qSlopeVector[pointIndex] != -1)
		{
			if((qSlopeVector[pointIndex] - initSlope + 8) % 8 >= flexibilityIndex && (initSlope - qSlopeVector[pointIndex] + 8) % 8 >= flexibilityIndex)
			{
				dominantPts.push_back(pointIndex);
			}

			initSlope = qSlopeVector[pointIndex];
		}

	}

	dominantPts.push_back(qSlopeVector.size() - 1);		//adding the last point

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Exiting DTWFeatureExtractor::DetermineDominantPoints" <<endl;

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 19-MAR-2005
* NAME			: computeTraceLength
* DESCRIPTION	: computes the length of the given trace between two given point indices
* ARGUMENTS		: trace - trace whose total/partial length is required
*				: fromPoint - point from which the trace length has to be computed
*				: toPoint - point to which the trace length has to be computed
* RETURNS		: total/partial length of the trace
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* Balaji MNA        4-Aug-2008          Check for argument point index.
*************************************************************************************/

int LTKPreprocessor::computeTraceLength(const LTKTrace& trace, int fromPoint, int toPoint, float& outLength)
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Entered LTKPreprocessor::computeTraceLength" <<endl;

	int NoOfPoints = trace.getNumberOfPoints();

	if( (fromPoint < 0 || fromPoint > (NoOfPoints-1))
		|| (toPoint < 0 || toPoint > (NoOfPoints-1)))
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
        <<"Error : "<< EPOINT_INDEX_OUT_OF_BOUND <<":"<< getErrorMessage(EPOINT_INDEX_OUT_OF_BOUND)
        <<"LTKPreprocessor::computeTraceLength" <<endl;

		LTKReturnError(EPOINT_INDEX_OUT_OF_BOUND);
	}

	int pointIndex;

	float xDiff, yDiff;

	float pointDistance;

	outLength = 0;

	floatVector xVec, yVec;

	int errorCode;

	if( (errorCode = trace.getChannelValues(X_CHANNEL_NAME, xVec))!= SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
		<<"Error: LTKPreprocessor::computeTraceLength"<<endl;
		LTKReturnError(errorCode);
	}


	if( (errorCode = trace.getChannelValues(Y_CHANNEL_NAME, yVec)) != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
		<<"Error: LTKPreprocessor::computeTraceLength"<<endl;
		LTKReturnError(errorCode);
	}

	for(pointIndex = fromPoint; pointIndex < toPoint; ++pointIndex)
	{
		xDiff = xVec[pointIndex] - xVec[pointIndex+1];

		yDiff = yVec[pointIndex] - yVec[pointIndex+1];

		pointDistance = (float) (sqrt(xDiff*xDiff + yDiff*yDiff)); //distance between 2 points.

		outLength += pointDistance; // finding the length of trace.

		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "point distance: " <<  pointDistance <<endl;
	}

    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Exiting LTKPreprocessor::computeTraceLength" <<endl;
	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 19-MAR-2005
* NAME			: removeDuplicatePoints
* DESCRIPTION	: remove consecutively repeating x, y coordinates (thinning)
* ARGUMENTS		: inTraceGroup - trace group which has to be thinned
*				: outTraceGroup - thinned trace group
* RETURNS		: SUCCESS on successful thinning operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

int LTKPreprocessor::removeDuplicatePoints(const LTKTraceGroup& inTraceGroup,
                                               LTKTraceGroup& outTraceGroup)
{
	int errorCode;
	int numTraces;

	int traceIndex;

	int pointIndex;

	int numPoints;

	floatVector newXVec, newYVec;

	numTraces = inTraceGroup.getNumTraces();

    vector<LTKTrace> tempTraceVector;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "numTraces = " <<  numTraces <<endl;

	for(traceIndex = 0; traceIndex < numTraces; ++traceIndex)
	{
		LTKTrace trace;

		inTraceGroup.getTraceAt(traceIndex, trace);

		floatVector xVec, yVec;

		if( (errorCode = trace.getChannelValues(X_CHANNEL_NAME, xVec))!= SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			<<"Error: LTKPreprocessor::removeDuplicatePoints"<<endl;
			LTKReturnError(errorCode);
		}


		if( (errorCode = trace.getChannelValues(Y_CHANNEL_NAME, yVec)) != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			<<"Error: LTKPreprocessor::removeDuplicatePoints"<<endl;
			LTKReturnError(errorCode);
		}

		numPoints = trace.getNumberOfPoints();

		if(numPoints > 0)
		{
			newXVec.push_back(xVec[0]);

			newYVec.push_back(yVec[0]);
		}

		for(pointIndex = 1; pointIndex < numPoints; ++pointIndex)
		{
			if((xVec[pointIndex] != xVec[pointIndex - 1]) || (yVec[pointIndex] != yVec[pointIndex - 1]))
			{
				newXVec.push_back(xVec[pointIndex]);

				newYVec.push_back(yVec[pointIndex]);
			}
		}

		float2DVector allChannelValuesVec;
		allChannelValuesVec.push_back(newXVec);
		allChannelValuesVec.push_back(newYVec);

		LTKTrace newTrace;
		if( (errorCode = newTrace.setAllChannelValues(allChannelValuesVec)) != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			<<"Error: LTKPreprocessor::removeDuplicatePoints"<<endl;
			LTKReturnError(errorCode);
		}

        tempTraceVector.push_back(newTrace);

		newXVec.clear();

		newYVec.clear();
	}


    LTKTraceGroup tempTraceGroup(tempTraceVector,
                                 inTraceGroup.getXScaleFactor(),
                                 inTraceGroup.getYScaleFactor());

    outTraceGroup = tempTraceGroup;

    return SUCCESS;
}


/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 19-MAR-2005
* NAME			: calculateSweptAngle
* DESCRIPTION	: calculates the swept angle of the curve.
* ARGUMENTS		: Input trace
* RETURNS		: Swept angle of the trace
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* Balaji MNA        4-Aug-2008          Define new error code ESINGLE_POINT_TRACE.
*************************************************************************************/

int LTKPreprocessor::calculateSweptAngle(const LTKTrace& trace, float& outSweptAngle)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Entered DTWFeatureExtractor::calculateSweptAngle" <<endl;

	outSweptAngle = 0.0;

	float prevSlope;

	float slope;				//Is a temporary variable which stores the slope values computed at each coordinate.

	int dimension;				//Stores the dimension of the CoordinatePoints

	float dx,dy;				//Temporary variales.

	int pointIndex;

	floatVector xVec, yVec;

	int errorCode;

	if( (errorCode = trace.getChannelValues(X_CHANNEL_NAME, xVec))!= SUCCESS)
    {
	    LOG(LTKLogger::LTK_LOGLEVEL_ERR)
        <<"Error: LTKPreprocessor::calculateSweptAngle"<<endl;
        LTKReturnError(errorCode);
    }


	if( (errorCode = trace.getChannelValues(Y_CHANNEL_NAME, yVec)) != SUCCESS)
    {
	    LOG(LTKLogger::LTK_LOGLEVEL_ERR)
        <<"Error: LTKPreprocessor::calculateSweptAngle"<<endl;
        LTKReturnError(errorCode);
    }

	dimension = trace.getNumberOfPoints(); //character.size();

	if(dimension < 2)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)
              <<"Error : "<< ESINGLE_POINT_TRACE <<":"<< getErrorMessage(ESINGLE_POINT_TRACE)
              <<"LTKPreprocessor::calculateSweptAngle" <<endl;
        LTKReturnError(ESINGLE_POINT_TRACE);
    }


	dx = xVec[1] - xVec[0];

	dy = yVec[1] - yVec[0];

	prevSlope = (float)atan2(dy, dx)*(180/3.14);

	for(pointIndex=1; pointIndex < dimension-1; ++pointIndex)
	{
		dx = xVec[pointIndex+1] - xVec[pointIndex];

		dy = yVec[pointIndex+1] - yVec[pointIndex];

		slope = (float)atan2(dy, dx)*(180/3.14);

		outSweptAngle = slope - prevSlope;

		prevSlope = slope;
	}

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Vijayakumara M.
* DATE			: 13-JULY-2005
* NAME			: LTKPreprocessor
* DESCRIPTION	: This function will add the function name and its address to the
*				  maping variable.
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

void LTKPreprocessor::initFunAddrMap ()
{

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<< "Entered default constructor of LTKPreprocessor" <<endl;

	string normalizeSize = NORMALIZE_FUNC;
    string removeDuplicatePoints = REMOVE_DUPLICATE_POINTS_FUNC;
	string smoothenTraceGroup = SMOOTHEN_TRACE_GROUP_FUNC;
	string dehookTraces = DEHOOKTRACES_FUNC;
	string normalizeOrientation = NORMALIZE_ORIENTATION_FUNC;
	string resampleTraceGroup = RESAMPLE_TRACE_GROUP_FUNC;

	m_preProcMap[normalizeSize]=&LTKPreprocessorInterface::normalizeSize;
	m_preProcMap[removeDuplicatePoints]=&LTKPreprocessorInterface::removeDuplicatePoints;
	m_preProcMap[smoothenTraceGroup]=&LTKPreprocessorInterface::smoothenTraceGroup;
	m_preProcMap[dehookTraces]=&LTKPreprocessorInterface::dehookTraces;
	m_preProcMap[normalizeOrientation]=&LTKPreprocessorInterface::normalizeOrientation;
    m_preProcMap[resampleTraceGroup]=&LTKPreprocessorInterface::resampleTraceGroup;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Exiting default constructor of LTKPreprocessor" <<endl;

}

/**********************************************************************************
* AUTHOR		: Vijayakumara M.
* DATE			: 13-JULY-2005
* NAME			: getPreprocptr
* DESCRIPTION	: This function returs the preprocessor function pointer.
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

FN_PTR_PREPROCESSOR LTKPreprocessor::getPreprocptr(const string &funName)
{
	return m_preProcMap[funName];
}

/**********************************************************************************
* AUTHOR		: Thanigai
* DATE			: 20-SEP-2005
* NAME			: initPreprocFactoryDefaults
* DESCRIPTION	: This function assigns the factory default values for the
*				  preprocessor attributes
* ARGUMENTS		: void
* RETURNS		: 0 on success
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
void LTKPreprocessor::initPreprocFactoryDefaults()
{
    m_sizeThreshold             = PREPROC_DEF_SIZE_THRESHOLD;
    m_aspectRatioThreshold      = PREPROC_DEF_ASPECTRATIO_THRESHOLD;
    m_dotThreshold              = PREPROC_DEF_DOT_THRESHOLD;
    m_loopThreshold             = PREPROC_DEF_LOOP_THRESHOLD;
    m_hookLengthThreshold1      = PREPROC_DEF_HOOKLENGTH_THRESHOLD1;
    m_hookLengthThreshold2      = PREPROC_DEF_HOOKLENGTH_THRESHOLD2;
    m_hookAngleThreshold        = PREPROC_DEF_HOOKANGLE_THRESHOLD;
    m_filterLength              = PREPROC_DEF_FILTER_LENGTH;
    m_preserveAspectRatio       = PREPROC_DEF_PRESERVE_ASPECT_RATIO;
    m_preserveRelativeYPosition = PREPROC_DEF_PRESERVE_RELATIVE_Y_POSITION;
    m_quantizationStep          = PREPROC_DEF_QUANTIZATIONSTEP;
    m_resamplingMethod          = PREPROC_DEF_RESAMPLINGMETHOD;
	m_traceDimension			= PREPROC_DEF_TRACE_DIMENSION;

	m_interPointDist = (float)PREPROC_DEF_NORMALIZEDSIZE/(float)PREPROC_DEF_INTERPOINT_DIST_FACTOR;

}

/**********************************************************************************
* AUTHOR		: Dinesh M
* DATE			: 30-May-2007
* NAME			: resampleTraceGroup
* DESCRIPTION	: This function resamples the tracegroup
* ARGUMENTS		: Input tracegroup, Output tracegroup
* RETURNS		: 0 on success
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
int LTKPreprocessor::resampleTraceGroup(const LTKTraceGroup& inTraceGroup,
											 LTKTraceGroup& outTraceGroup)
{
	int errorCode;
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<  "Entered LTKPreprocessor::resampleTraceGroup1" <<endl;
	int total = 0;
	float totalLength = 0;
	int totalPoints = 0;

	const vector<LTKTrace>& tracesVector = inTraceGroup.getAllTraces();
    vector<LTKTrace> tempTraceVector;

	int numberOfTraces = tracesVector.size();

	if(numberOfTraces==0)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)
              <<"Error : "<< EEMPTY_TRACE_GROUP <<" : "<<
              getErrorMessage(EEMPTY_TRACE_GROUP)
              <<"LTKPreprocessor::resampleTraceGroup" <<endl;

        LTKReturnError(EEMPTY_TRACE_GROUP);
    }

	vector<int> pointsPerTrace(numberOfTraces, 0);

	if(m_resamplingMethod == LENGTHBASED)
	{
		int maxIndex = 0;
		float maxLength = 0.0;

		floatVector lengthsVec(numberOfTraces, 0.0f);

		for(int j=0; j < numberOfTraces; ++j)
		{
			float length;
			const LTKTrace& trace = tracesVector[j];
			if( (errorCode = computeTraceLength(trace, 0, trace.getNumberOfPoints() - 1,length))!= SUCCESS)
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)
				<<"Error: LTKPreprocessor::resampleTraceGroup"<<endl;
				LTKReturnError(errorCode);
			}
			lengthsVec[j] = length;
			if(fabs(lengthsVec[j]) < EPS)
			{
				lengthsVec[j] = EPS;
			}
			totalLength += lengthsVec[j];
			if(lengthsVec[j] > maxLength)
			{
				maxLength = lengthsVec[j];
				maxIndex = j;
			}
		}


		if(m_quantizationStep == 1)
		{
			for(int i = 0; i < numberOfTraces; ++i)
			{
				if( i == (numberOfTraces -1))
					pointsPerTrace[i] = ((m_traceDimension - numberOfTraces) - total);
				else
				{
					pointsPerTrace[i] = int(floor((m_traceDimension - numberOfTraces) *
					lengthsVec[i] / totalLength));
				}

				total += pointsPerTrace[i];

			}
		}
		else
		{
			for(int i = 0; i < numberOfTraces; ++i)
			{
				if(i != maxIndex)
				{
					pointsPerTrace[i] =  m_quantizationStep * int(floor((m_traceDimension * lengthsVec[i] / (m_quantizationStep * totalLength)) + 0.5f));

					if(pointsPerTrace[i] <= 0)
					{
						pointsPerTrace[i] = 1;
					}

					total += pointsPerTrace[i];
				}
			}
			pointsPerTrace[maxIndex] = m_traceDimension - total;

		}
		int sum =0;
		for(int temp =0; temp < pointsPerTrace.size(); ++temp)
		{
		 sum+=pointsPerTrace[temp];
		}

	}
	else if(m_resamplingMethod == POINTBASED)
	{
		for(int j=0; j < numberOfTraces; ++j)
			totalPoints += tracesVector[j].getNumberOfPoints();

		if(totalPoints==0)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
				  <<"Error : "<< EEMPTY_TRACE_GROUP <<":"<< getErrorMessage(EEMPTY_TRACE_GROUP)
				  <<"LTKPreprocessor::resampleTraceGroup" <<endl;

			LTKReturnError(EEMPTY_TRACE_GROUP);
		}

		for(int i=0; i < numberOfTraces; ++i)
		{
			const LTKTrace& trace = tracesVector.at(i);
			if( i == (numberOfTraces - 1))
				pointsPerTrace[i] = ((m_traceDimension - numberOfTraces) - total);
			else
			{
				pointsPerTrace[i] = int(floor((m_traceDimension - numberOfTraces) * trace.getNumberOfPoints() / float(totalPoints) ));
			}

			total += pointsPerTrace[i];

		}

	}
	else if(m_resamplingMethod == INTERPOINTDISTBASED)
	{


		for(int j=0; j < numberOfTraces; ++j)
		{
			float length;
			LTKTrace trace = tracesVector[j];

			LTKTraceGroup tempTraceGroup(trace,inTraceGroup.getXScaleFactor(),inTraceGroup.getYScaleFactor());

			if(isDot(tempTraceGroup))
			{
				pointsPerTrace[j] = -1;
			}
			else
			{

				if( (errorCode = computeTraceLength(trace, 0, trace.getNumberOfPoints() - 1,length))!= SUCCESS)
				{
					LOG(LTKLogger::LTK_LOGLEVEL_ERR)
					<<"Error: LTKPreprocessor::resampleTraceGroup"<<endl;
					LTKReturnError(errorCode);
				}

				pointsPerTrace[j] = ((int)floor((length/m_interPointDist)+0.5f)) + 1;  //0.5f is for rounding the result to the nearest integer



			}
		}
	}
	else
	{
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)
				  <<"Error : "<< ECONFIG_FILE_RANGE <<":"<< getErrorMessage(ECONFIG_FILE_RANGE)
				  <<"LTKPreprocessor::resampleTraceGroup" <<endl;

			LTKReturnError(ECONFIG_FILE_RANGE);

	}

	for(int i=0; i < numberOfTraces; ++i)
	{
		LTKTrace newTrace;
		const LTKTrace& trace = tracesVector.at(i);
		if(m_resamplingMethod == "lengthbased" && m_quantizationStep != 1)
		{
			if( (errorCode = resampleTrace(trace,pointsPerTrace[i], newTrace))!= SUCCESS)
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)
				<<"Error: LTKPreprocessor::resampleTraceGroup"<<endl;
				LTKReturnError(errorCode);
			}
		}
		else if(m_resamplingMethod == "interpointdistbased")
		{
			if(pointsPerTrace[i] == -1) //if the trace corresponds to a dot
			{
				vector<float> avgPoint;


				if( (errorCode = resampleTrace(trace,1, newTrace))!= SUCCESS)
				{
					LOG(LTKLogger::LTK_LOGLEVEL_ERR)
					<<"Error: LTKPreprocessor::resampleTraceGroup"<<endl;
					LTKReturnError(errorCode);
				}

				if( (errorCode = newTrace.getPointAt(0,avgPoint))!= SUCCESS)
				{
					LOG(LTKLogger::LTK_LOGLEVEL_ERR)
					<<"Error: LTKPreprocessor::resampleTraceGroup"<<endl;
					LTKReturnError(errorCode);
				}

					//Adding the average point 5 times in the new trace
				for(int p=0;p<4;++p)
				{
					newTrace.addPoint(avgPoint);
				}



			}
			else
			{
				if( (errorCode = resampleTrace(trace,pointsPerTrace[i]+1, newTrace))!= SUCCESS)
				{
					LOG(LTKLogger::LTK_LOGLEVEL_ERR)
					<<"Error: LTKPreprocessor::resampleTraceGroup"<<endl;
					LTKReturnError(errorCode);
				}
			}
		}
		else
		{
			if( (errorCode = resampleTrace(trace,pointsPerTrace[i]+1, newTrace))!= SUCCESS)
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)
				<<"Error: LTKPreprocessor::resampleTraceGroup"<<endl;
				LTKReturnError(errorCode);
			}
		}
        tempTraceVector.push_back(newTrace);
	}


    outTraceGroup.setAllTraces(tempTraceVector,
                                 inTraceGroup.getXScaleFactor(),
                                 inTraceGroup.getYScaleFactor());

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<  "Exiting LTKPreprocessor::resampleTraceGroup1" <<endl;

	return SUCCESS;

}

/**********************************************************************************
* AUTHOR		: Nidhi Sharma
* DATE			: 13-Sept-2007
* NAME			: setTraceDimension
* DESCRIPTION	:
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* Nidhi Sharma      13-Sept-2007        Added a check on function parameter, assignment happens
*                                                       only if input parameter is on zero
*************************************************************************************/
int LTKPreprocessor::setTraceDimension(int traceDimension)
{
	if(traceDimension <= 0)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)
              <<"Error : "<< ENON_POSITIVE_NUM <<":"<< getErrorMessage(ENON_POSITIVE_NUM)
              <<"LTKPreprocessor::setTraceDimension" <<endl;

        LTKReturnError(ENON_POSITIVE_NUM);
    }
	m_traceDimension = traceDimension;

    return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Nidhi Sharma
* DATE			: 13-Sept-2007
* NAME			: initialize
* DESCRIPTION	:
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
int LTKPreprocessor::readConfig(const string& cfgFilePath)
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Entering LTKPreprocessor::readConfig" <<endl;

    LTKConfigFileReader* configurableProperties = NULL;
    string tempStringVar = "";


    try
    {
        configurableProperties = new LTKConfigFileReader(cfgFilePath);
	}
	catch(LTKException e)
    {
        delete configurableProperties;

        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "
		    << e.getErrorCode() <<" LTKPreprocessor::readConfig()"<<endl;

        LTKReturnError(e.getErrorCode());
    }

   // Read the key-values pairs defined in the cfg file

    /*
        *   Trace Dimension
        */
    int errorCode = FAILURE;

	try
	{
	    errorCode = configurableProperties->getConfigValue(TRACEDIMENSION,
	                                                       tempStringVar);
	    if( errorCode == SUCCESS )
	    {
	        if ( LTKStringUtil::isInteger(tempStringVar) )
	        {
	            if (setTraceDimension(atoi((tempStringVar).c_str())) != SUCCESS)
	            {

	                LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << ECONFIG_FILE_RANGE
	    				<<" LTKPreprocessor::readConfig()"<<endl;
	                    throw LTKException(ECONFIG_FILE_RANGE);
	            }
	        }
	        else
	        {
	            LOG(LTKLogger::LTK_LOGLEVEL_ERR) <<
	                "Error: " << ECONFIG_FILE_RANGE << " : " << TRACEDIMENSION <<
	                " is out of permitted range" <<
	                " LTKPreprocessor::readConfig()"<<endl;

	                throw LTKException(ECONFIG_FILE_RANGE);
	        }
	    }

	    /*
	            * Size Threshold
	            */
	    tempStringVar = "";
	    errorCode = configurableProperties->getConfigValue(SIZETHRESHOLD,
	                                                       tempStringVar);

		if( errorCode == SUCCESS)
		{
		    if ( LTKStringUtil::isFloat(tempStringVar) )
	        {
                if (setSizeThreshold(LTKStringUtil::convertStringToFloat(tempStringVar)) != SUCCESS)
	            {
	                LOG(LTKLogger::LTK_LOGLEVEL_ERR) << SIZETHRESHOLD <<
	                    " should be atleast less than 1/100'th of NORMALIZEDSIZE"<<endl;

	    			LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << ECONFIG_FILE_RANGE
	    				<<" LTKPreprocessor::readConfig()"<<endl;
	                    throw LTKException(ECONFIG_FILE_RANGE);
	            }
	       }
	        else
	        {
	            LOG(LTKLogger::LTK_LOGLEVEL_ERR) <<
	                "Error: " << ECONFIG_FILE_RANGE <<
	                " Invalid value " << tempStringVar << " for " << SIZETHRESHOLD <<
	    			" LTKPreprocessor::readConfig()" << endl;

	            throw LTKException(ECONFIG_FILE_RANGE);
	        }
		}

	    /*
	            * Aspect ratio threshold
	            */

	    tempStringVar = "";
		errorCode = configurableProperties->getConfigValue(ASPECTRATIOTHRESHOLD,
	                                                       tempStringVar);

		if( errorCode == SUCCESS)
		{
		    if ( LTKStringUtil::isFloat(tempStringVar) )
	        {
                if (setAspectRatioThreshold(LTKStringUtil::convertStringToFloat(tempStringVar)) != SUCCESS)
	            {
	                LOG( LTKLogger::LTK_LOGLEVEL_ERR) <<
	                    ASPECTRATIOTHRESHOLD << " should be positive" << endl;

	    			LOG(LTKLogger::LTK_LOGLEVEL_ERR) <<
	                    "Error: " << ECONFIG_FILE_RANGE
	    				<<" LTKPreprocessor::readConfig()"<<endl;
	                    throw LTKException(ECONFIG_FILE_RANGE);

	            }
	        }
	        else
	        {
	            LOG(LTKLogger::LTK_LOGLEVEL_ERR) <<
	                "Error: " << ECONFIG_FILE_RANGE << " Invalid value " <<
	                tempStringVar << "for " << ASPECTRATIOTHRESHOLD <<
	    			" LTKPreprocessor::readConfig()" << endl;

	            throw LTKException(ECONFIG_FILE_RANGE);
	        }
		}

	    /*
	            * Dot Threshold
	            */

	    tempStringVar = "";
		errorCode = configurableProperties->getConfigValue(DOTTHRESHOLD,
	                                                       tempStringVar);

		if( errorCode == SUCCESS)
		{
		    if ( LTKStringUtil::isFloat(tempStringVar) )
	        {
                if (setDotThreshold(LTKStringUtil::convertStringToFloat(tempStringVar)) != SUCCESS)
	            {
	                LOG(LTKLogger::LTK_LOGLEVEL_ERR) << DOTTHRESHOLD <<
	                    " should be positive" << endl ;

	                LOG(LTKLogger::LTK_LOGLEVEL_ERR) <<
	                    "Error: " << ECONFIG_FILE_RANGE  <<
	                    " LTKPreprocessor::readConfig()"<<endl;

	                 throw LTKException(ECONFIG_FILE_RANGE);
	            }
	        }
	        else
	        {
	            LOG(LTKLogger::LTK_LOGLEVEL_ERR) <<
	                "Error: " << ECONFIG_FILE_RANGE <<
	                " Invalid value " << tempStringVar << " for " << DOTTHRESHOLD <<
	    		    " LTKPreprocessor::readConfig()"<<endl;

	            throw LTKException(ECONFIG_FILE_RANGE);
	        }
		}

	    /*
	    * Preserve relative y position
	    */

	    tempStringVar = "";
		configurableProperties->getConfigValue(PRESERVERELATIVEYPOSITION,
	                                           tempStringVar);

		if(LTKSTRCMP(tempStringVar.c_str(), "true") == 0)
		{
			m_preserveRelativeYPosition = true;
			LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<< PRESERVERELATIVEYPOSITION <<
	            " turned on during normalization"<<endl;
		}
		else if(LTKSTRCMP(tempStringVar.c_str(), "false") == 0)
		{
			m_preserveRelativeYPosition = false;
			LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<< PRESERVERELATIVEYPOSITION <<
	            " turned on during normalization"<<endl;
		}
		else
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR) <<
	                "Error: " << ECONFIG_FILE_RANGE <<
	                " Invalid value " << tempStringVar << " for " << PRESERVERELATIVEYPOSITION <<
	    		    " LTKPreprocessor::readConfig()"<<endl;

	        throw LTKException(ECONFIG_FILE_RANGE);
		}

	    /*
	    * Preserve aspect ratio
	    */

	    tempStringVar = "";
		configurableProperties->getConfigValue(PRESERVEASPECTRATIO,
	                                           tempStringVar);

		if(LTKSTRCMP(tempStringVar.c_str(), "false") ==0)
		{
		    setPreserveAspectRatio(false);
		}
		else if (LTKSTRCMP(tempStringVar.c_str(), "true") ==0)
		{
		    setPreserveAspectRatio(true);
		}
		else
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR) <<
	                "Error: " << ECONFIG_FILE_RANGE <<
	                " Invalid value " << tempStringVar << " for " << PRESERVEASPECTRATIO <<
	    		    " LTKPreprocessor::readConfig()"<<endl;

	        throw LTKException(ECONFIG_FILE_RANGE);
		}

	    /*
	            * Resampling method
	            */
	    tempStringVar = "";
		errorCode = configurableProperties->getConfigValue(RESAMPLINGMETHOD,
	                                                       tempStringVar);

		if( errorCode == SUCCESS)
		{
		    if (setResamplingMethod(tempStringVar) != SUCCESS)
	        {
	            LOG(LTKLogger::LTK_LOGLEVEL_ERR)<< "ERROR: " <<
	                " Invalid value " << tempStringVar << " for " <<
	                RESAMPLINGMETHOD << " LTKPreprocessor::readConfig()" << endl;

	           LOG(LTKLogger::LTK_LOGLEVEL_ERR) <<
	               "Error: " << ECONFIG_FILE_RANGE
					<<" LTKPreprocessor::readConfig()"<<endl;
	                throw LTKException(ECONFIG_FILE_RANGE);
	        }
	    }

    /*
            * Quantation step, needed only f the resampling method is length based
            */
	    if ( LTKSTRCMP(m_resamplingMethod.c_str(), LENGTHBASED) == 0 )
	    {
	        tempStringVar = "";
		    errorCode = configurableProperties->getConfigValue(QUANTIZATIONSTEP,
	                                                           tempStringVar);

	        if( errorCode == SUCCESS)
			{
			    if ( LTKStringUtil::isInteger(tempStringVar) )
	            {
	    		    if (setQuantizationStep(atoi((tempStringVar).c_str())) != SUCCESS)
	                {
	                    LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
	                        QUANTIZATIONSTEP << " greater than one "<< endl;

	                    LOG(LTKLogger::LTK_LOGLEVEL_ERR) <<
	                    "Error: " << ECONFIG_FILE_RANGE
	    				<<" LTKPreprocessor::readConfig()"<<endl;
	                    throw LTKException(ECONFIG_FILE_RANGE);
	                }
	            }
	            else
	            {
	                LOG(LTKLogger::LTK_LOGLEVEL_ERR) <<
	                    "Error: " << ECONFIG_FILE_RANGE <<
	                    " Invalid value " << tempStringVar << "for " <<
	                    QUANTIZATIONSTEP <<
	    				" LTKPreprocessor::readConfig()"<<endl;

	                throw LTKException(ECONFIG_FILE_RANGE);
	            }
	        }
	    }

        /*
        * SmoothWindowSize method
        */
	    tempStringVar = "";
		errorCode = configurableProperties->getConfigValue(SMOOTHFILTERLENGTH,
	                                                       tempStringVar);

		if( errorCode == SUCCESS)
		{
		    if ( LTKStringUtil::isInteger(tempStringVar) )
            {
    		    if (setFilterLength(atoi((tempStringVar).c_str())) != SUCCESS)
                {
                    LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                        SMOOTHFILTERLENGTH << " greater than one "<< endl;

                    LOG(LTKLogger::LTK_LOGLEVEL_ERR) <<
                    "Error: " << ECONFIG_FILE_RANGE
    				<<" LTKPreprocessor::readConfig()"<<endl;
                    throw LTKException(ECONFIG_FILE_RANGE);
                }
            }
            else
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR) <<
                    "Error: " << ECONFIG_FILE_RANGE <<
                    " Invalid value " << tempStringVar << "for " <<
                    SMOOTHFILTERLENGTH <<
    				" LTKPreprocessor::readConfig()"<<endl;

                throw LTKException(ECONFIG_FILE_RANGE);
            }
	    }
	}
	catch(LTKException e)
	{
		delete configurableProperties;
		LTKReturnError(e.getErrorCode());
	}


    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Exiting LTKPreprocessor::initialize" <<endl;

    delete configurableProperties;
	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Nidhi Sharma
* DATE			: 13-Sept-2007
* NAME			: initialize
* DESCRIPTION	:
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
LTKPreprocessor::LTKPreprocessor(const LTKControlInfo& controlInfo)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Entered default constructor of LTKPreprocessor" <<endl;

	initFunAddrMap();
	initPreprocFactoryDefaults();

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Exiting default constructor of LTKPreprocessor" <<endl;

    string cfgFilePath = "";

    // Config file
    if ( ! ((controlInfo.lipiRoot).empty()) &&
         ! ((controlInfo.projectName).empty()) &&
         ! ((controlInfo.profileName).empty()) &&
         ! ((controlInfo.cfgFileName).empty()))
    {
        // construct the cfg path using project and profile name
         cfgFilePath = (controlInfo.lipiRoot) + PROJECTS_PATH_STRING +
                       (controlInfo.projectName) + PROFILE_PATH_STRING +
                       (controlInfo.profileName) + SEPARATOR +
                       (controlInfo.cfgFileName) + CONFIGFILEEXT;
    }
    else if ( ! ((controlInfo.cfgFilePath).empty() ))
    {
        cfgFilePath = controlInfo.cfgFilePath;
    }
    else
    {
        return;
    }

	int returnVal = readConfig(cfgFilePath);
    if (returnVal != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			<<"Error: LTKPreprocessor::LTKPreprocessor"<<endl;
		throw LTKException(returnVal);
	}
}

bool LTKPreprocessor::isDot(const LTKTraceGroup& inTraceGroup)
{

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Entered LTKPreprocessor::isDot" <<endl;

	float xMin,yMin,xMax,yMax;

	float xScale;						//	scale along the x direction

	float yScale;						//	scale along the y direction

    int errorCode;

	// getting the bounding box information of the input trace group

	if( (errorCode = inTraceGroup.getBoundingBox(xMin,yMin,xMax,yMax)) != SUCCESS)
    {
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
        <<"Error: LTKPreprocessor::isDot"<<endl;
        LTKReturnError(errorCode);
	}


	//	width of the bounding box at scalefactor = 1

	xScale = ((float)fabs(xMax - xMin))/inTraceGroup.getXScaleFactor();

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "xScale = " <<  xScale <<endl;

	// height of the bounding box at scalefactor = 1

	yScale = ((float)fabs(yMax - yMin))/inTraceGroup.getYScaleFactor();

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "yScale = " <<  yScale <<endl;


	if(xScale <= (m_dotThreshold * m_captureDevice.getXDPI()) && yScale <= (m_dotThreshold * m_captureDevice.getYDPI()))
	{
		LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Exiting LTKPreprocessor::isDot" <<endl;
		return true;
	}
	else
	{
		LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<   "Exiting LTKPreprocessor::isDot" <<endl;
		return false;
	}

}
