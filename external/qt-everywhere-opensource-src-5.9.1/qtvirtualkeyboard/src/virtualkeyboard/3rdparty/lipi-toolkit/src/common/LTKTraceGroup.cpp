/******************************************************************************
* Copyright (c) 2006 Hewlett-Packard Development Company, L.P.
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE
********************************************************************************/

/******************************************************************************
 * SVN MACROS
 *
 * $LastChangedDate: 2008-07-18 15:00:39 +0530 (Fri, 18 Jul 2008) $
 * $Revision: 561 $
 * $Author: sharmnid $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Implementation of LTKTraceGroup which holds a sequence of LTKTrace type objects
 *
 * CONTENTS:
 *	getAllTraces
 *	getTraceAt
 *	getNumTraces
 *	addTrace
 *	setAllTraces
 *
 * AUTHOR:     Balaji R.
 *
 * DATE:       December 23, 2004
 * CHANGE HISTORY:
 * Author		Date			Description of change
 * Deepu V.     March 7, 2005   Added new assignment operator (from LTKTrace)
 *                              and copy constructor (from LTKTrace )
 * Thanigai		09-AUG-2005		Added a to empty the trace group
************************************************************************/

#include "LTKTraceGroup.h"
#include "LTKTrace.h"
#include "LTKErrors.h"
#include "LTKErrorsList.h"
#include "LTKLoggerUtil.h"
#include "LTKException.h"
#include "LTKMacros.h"

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: LTKTraceGroup
* DESCRIPTION	: Default Constructor
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/
LTKTraceGroup::LTKTraceGroup() :
	m_xScaleFactor(1.0),
	m_yScaleFactor(1.0)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Enter: LTKTraceGroup::LTKTraceGroup()"<<endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Enter: LTKTraceGroup::LTKTraceGroup()"<<endl;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: LTKTraceGroup
* DESCRIPTION	: Initialization constructor
* ARGUMENTS		: inTraceVector - vector of traces to be set to class member
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/
LTKTraceGroup::LTKTraceGroup(const LTKTraceVector& inTraceVector,
                             float xScaleFactor, float yScaleFactor) :
	m_traceVector(inTraceVector)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Enter: LTKTraceGroup::LTKTraceGroup(LTKTraceVector&, float,float)"<<endl;

	if(xScaleFactor <= 0)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) <<
        "Error: "<<EINVALID_X_SCALE_FACTOR <<": "<<
		getErrorMessage(EINVALID_X_SCALE_FACTOR) <<
		"LTKTraceGroup::LTKTraceGroup(LTKTraceVector&, float,float)"<<endl;

		throw LTKException(EINVALID_X_SCALE_FACTOR);
	}

	if(yScaleFactor <= 0)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) <<
        "Error: "<<EINVALID_Y_SCALE_FACTOR <<": "<<
		getErrorMessage(EINVALID_Y_SCALE_FACTOR) <<
		"LTKTraceGroup::LTKTraceGroup(LTKTraceVector&, float,float)"<<endl;

		throw LTKException(EINVALID_Y_SCALE_FACTOR);
	}

	m_xScaleFactor = xScaleFactor;
	m_yScaleFactor = yScaleFactor;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Exit: LTKTraceGroup::LTKTraceGroup(LTKTraceVector&, float,float)"<<endl;


}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: LTKTraceGroup
* DESCRIPTION	: Copy Constructor
* ARGUMENTS		: traceGroup - trace group to be copied
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

LTKTraceGroup::LTKTraceGroup(const LTKTraceGroup& traceGroup)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Enter: LTKTraceGroup::LTKTraceGroup(LTKTraceGroup&)"<<endl;

	m_traceVector = traceGroup.m_traceVector;
	m_xScaleFactor = traceGroup.m_xScaleFactor;
	m_yScaleFactor = traceGroup.m_yScaleFactor;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Exit: LTKTraceGroup::LTKTraceGroup(LTKTraceGroup&)"<<endl;

}

/******************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 07-MAR-2005
* NAME			: LTKTraceGroup
* DESCRIPTION	: Constructor from LTKTrace
* ARGUMENTS		: trace - trace object to be copied
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

LTKTraceGroup::LTKTraceGroup(const LTKTrace& trace,
                             float xScaleFactor, float yScaleFactor)
{

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Enter:LTKTraceGroup::LTKTraceGroup(LTKTraceGroup&, float, float)"<<endl;

	if(xScaleFactor <= 0)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) <<
        "Error: "<<EINVALID_X_SCALE_FACTOR <<": "<<
		getErrorMessage(EINVALID_X_SCALE_FACTOR) <<
		"LTKTraceGroup::LTKTraceGroup(LTKTrace&, float,float)"<<endl;

		throw LTKException(EINVALID_X_SCALE_FACTOR);
	}

	if(yScaleFactor <= 0)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) <<
        "Error: "<<EINVALID_Y_SCALE_FACTOR <<": "<<
		getErrorMessage(EINVALID_Y_SCALE_FACTOR) <<
		"LTKTraceGroup::LTKTraceGroup(LTKTrace&, float,float)"<<endl;

		throw LTKException(EINVALID_Y_SCALE_FACTOR);
	}

	m_xScaleFactor = xScaleFactor;
	m_yScaleFactor = yScaleFactor;

	m_traceVector.push_back(trace);

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Exit:LTKTraceGroup::LTKTraceGroup(LTKTraceGroup&, float, float)"<<endl;

}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: operator=
* DESCRIPTION	: Overloaded assignment operator
* ARGUMENTS		: traceGroup - trace group to be assigned to
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

LTKTraceGroup& LTKTraceGroup::operator=(const LTKTraceGroup& traceGroup)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Enter: LTKTraceGroup:: operator =(LTKTraceGroup&)"<<endl;

	if(this != &traceGroup)
	{
		m_traceVector = traceGroup.m_traceVector;
		m_xScaleFactor = traceGroup.m_xScaleFactor;
		m_yScaleFactor = traceGroup.m_yScaleFactor;
	}


	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Exit: LTKTraceGroup:: operator =(LTKTraceGroup&)"<<endl;

	return *this;
}

/******************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 07-MAR-2005
* NAME			: operator=
* DESCRIPTION	: Overloaded assignment operator (From LTKTrace)
* ARGUMENTS		: trace - trace to be assigned
* RETURNS		: LTKTraceGroup&
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

LTKTraceGroup& LTKTraceGroup::operator=(const LTKTrace& trace)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Exit: LTKTraceGroup:: operator =(LTKTrace&)"<<endl;

	emptyAllTraces();
	m_traceVector.push_back(trace);

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Exit: LTKTraceGroup:: operator =(LTKTrace&)"<<endl;

	return *this;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getAllTraces
* DESCRIPTION	: gets traces composing the trace group as a vector
* ARGUMENTS		:
* RETURNS		: reference to traces composing the trace group as a vector
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

const LTKTraceVector& LTKTraceGroup::getAllTraces() const
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Enter: LTKTraceGroup::getAllTraces()"<<endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Exit: LTKTraceGroup:: getAllTraces()"<<endl;

	return m_traceVector;
}


/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getTraceAt
* DESCRIPTION	: gets the trace at the specified index
* ARGUMENTS		: traceIndex - index of the trace to return
* RETURNS		: reference to trace at the specified index
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

int LTKTraceGroup::getTraceAt(int traceIndex, LTKTrace& outTrace) const
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Enter: LTKTraceGroup::getTraceAt()"<<endl;

	if ( traceIndex < 0 || traceIndex >= m_traceVector.size() )
    {
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) <<
        "Error: "<<ETRACE_INDEX_OUT_OF_BOUND <<": "<<
		getErrorMessage(ETRACE_INDEX_OUT_OF_BOUND) <<
		"LTKTraceGroup::getTraceAt()"<<endl;


        LTKReturnError(ETRACE_INDEX_OUT_OF_BOUND);
    }

    outTrace = m_traceVector[traceIndex];

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Enter: LTKTraceGroup::getTraceAt()"<<endl;

    return SUCCESS;

}


/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getNumTraces
* DESCRIPTION	: gets the number of traces composing the trace group
* ARGUMENTS		:
* RETURNS		: number of traces composing the trace group
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

int LTKTraceGroup::getNumTraces () const
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Enter: LTKTraceGroup::getNumTraces()"<<endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"m_traceVector size = "<<m_traceVector.size()<<endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Exit: LTKTraceGroup::getNumTraces()"<<endl;

	return m_traceVector.size();
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: addTrace
* DESCRIPTION	: adds a trace to the trace group
* ARGUMENTS		: trace - trace to be added to the trace group
* RETURNS		: SUCCESS on successful addition of trace
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*******************************************************************************/

int LTKTraceGroup::addTrace(const LTKTrace& trace)
{

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Enter: LTKTraceGroup::addTrace()"<<endl;

	m_traceVector.push_back(trace);

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Exit: LTKTraceGroup::addTrace()"<<endl;

	return SUCCESS;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: setAllTraces
* DESCRIPTION	: reassigns the trace vector member variable
* ARGUMENTS		: traceVector - vector of traces to be reassigned to
* RETURNS		: SUCCESS on successful reassignment
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

int LTKTraceGroup::setAllTraces(const LTKTraceVector& traceVector, float xScaleFactor,
								float yScaleFactor)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Enter: LTKTraceGroup::setAllTraces()"<<endl;

	if (xScaleFactor <= 0 )
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) <<
        "Error: "<<EINVALID_X_SCALE_FACTOR <<": "<<
		getErrorMessage(EINVALID_X_SCALE_FACTOR) <<
		"LTKTraceGroup::setAllTraces()"<<endl;

		LTKReturnError(EINVALID_X_SCALE_FACTOR);
	}

	if (yScaleFactor <= 0 )
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) <<
        "Error: "<<EINVALID_Y_SCALE_FACTOR <<": "<<
		getErrorMessage(EINVALID_Y_SCALE_FACTOR) <<
		"LTKTraceGroup::setAllTraces()"<<endl;

		LTKReturnError(EINVALID_Y_SCALE_FACTOR);
	}


	m_traceVector = traceVector;
    m_xScaleFactor = xScaleFactor;
	m_yScaleFactor = yScaleFactor;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Exit: LTKTraceGroup::setAllTraces()"<<endl;

	return SUCCESS;
}


/******************************************************************************
* AUTHOR		: Thanigai
* DATE			: 09-AUG-2005
* NAME			: emptyAllTraces
* DESCRIPTION	: To empty the trace vector
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/
void LTKTraceGroup::emptyAllTraces()
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Enter: LTKTraceGroup::emptyAllTraces()"<<endl;

	m_traceVector.clear();
	m_xScaleFactor = 1.0;
	m_yScaleFactor = 1.0;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Exit: LTKTraceGroup::emptyAllTraces()"<<endl;

}


/******************************************************************************
* AUTHOR		: Dinesh M
* DATE			: 18-Apr-2007
* NAME			: getScale
* DESCRIPTION	:
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/
float LTKTraceGroup::getXScaleFactor() const
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Enter: LTKTraceGroup::getXScaleFactor()"<<endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Exit: LTKTraceGroup::getXScaleFactor()"<<endl;

	return m_xScaleFactor;
}


/******************************************************************************
* AUTHOR		: Dinesh M
* DATE			: 18-Apr-2007
* NAME			: getScale
* DESCRIPTION	:
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/
float LTKTraceGroup::getYScaleFactor() const
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Enter: LTKTraceGroup::getYScaleFactor()"<<endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Exit: LTKTraceGroup::getYScaleFactor()"<<endl;

	return m_yScaleFactor;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getBoundingBox
* DESCRIPTION	: gets the bounding box of the incoming trace group
* ARGUMENTS		: inTraceGroup - incoming trace group
* RETURNS		: SUCCESS on successful get operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*******************************************************************************/
int LTKTraceGroup::getBoundingBox(float& outXMin,float& outYMin,
                    float& outXMax,float& outYMax) const
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Enter: LTKTraceGroup::getBoundingBox()"<<endl;

	int numTraces = getNumTraces();	//	number of traces in the trace group

	int numPoints = -1;				//	number of points in a trace

	int pointIndex = -1;			//	variable to loop over points of a trace

	int errorCode;

	if ( numTraces == 0 )
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) <<
        "Error: "<<EEMPTY_TRACE_GROUP <<": "<<
		getErrorMessage(EEMPTY_TRACE_GROUP) <<
		"LTKTraceGroup::getBoundingBox()"<<endl;

		LTKReturnError(EEMPTY_TRACE_GROUP);
	}


	outXMin = outYMin = FLT_MAX;
	outXMax = outYMax = -FLT_MAX;


	for(int traceIndex = 0 ; traceIndex < numTraces; ++traceIndex)
	{
		const LTKTrace& tempTrace = m_traceVector[traceIndex];
		
       	floatVector xVec;
		
        errorCode = tempTrace.getChannelValues(X_CHANNEL_NAME, xVec);
        
        if ( errorCode != SUCCESS )
        {
			LOG( LTKLogger::LTK_LOGLEVEL_ERR) <<
	        "Error: LTKTraceGroup::getBoundingBox()"<<endl;

            LTKReturnError(errorCode);
        }

		floatVector yVec;
        errorCode= tempTrace.getChannelValues(Y_CHANNEL_NAME, yVec);
        if ( errorCode != SUCCESS )
        {
			LOG( LTKLogger::LTK_LOGLEVEL_ERR) <<
	        "Error: LTKTraceGroup::getBoundingBox()"<<endl;

            LTKReturnError(errorCode);
        }

		numPoints = xVec.size();

		for(pointIndex =0 ; pointIndex < numPoints; pointIndex++)
		{
			float x,y;

			x = xVec[pointIndex];
			y = yVec[pointIndex];


			if ( x < outXMin )
			{
				outXMin = x;
			}

			if ( x > outXMax )
			{
				outXMax = x;
			}

			if ( y < outYMin )
			{
				outYMin = y;
			}

			if ( y > outYMax )
			{
				outYMax = y;
			}
		}

	}

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Exit: LTKTraceGroup::getBoundingBox()"<<endl;


	return SUCCESS;

}


/******************************************************************************
* AUTHOR		: Bharath A.
* DATE			: 03-SEP-2007
* NAME			: scale
* DESCRIPTION	: scales the tracegroup according to the x and y scale factors.
*				  After scaling, the tracegroup is translated in order to
*				  preserve the "cornerToPreserve".
* ARGUMENTS		: xScaleFactor - factor by which x dimension has to be scaled
*				  yScaleFactor - factor by which y dimension has to be scaled
*				  cornerToPreserve - corner to be retained after scaling
* RETURNS		: SUCCESS on successful scale operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/
int LTKTraceGroup::scale(float xScaleFactor, float yScaleFactor,
                          TGCORNER cornerToPreserve)
{

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Enter: LTKTraceGroup::scale()"<<endl;



	LTKTrace trace;
	vector<LTKTrace> scaledTracesVec;	// holds the scaled traces

	floatVector scaledXVec;				//	scaled x channel values of a trace
	floatVector scaledYVec;				//	scaled y channel values of a trace

	float x=0.0f;
    float y=0.0f;
	float xToPreserve=0.0f;
    float yToPreserve=0.0f;
	float xMin=0.0f;
	float yMin=0.0f;
	float xMax=0.0f;
	float yMax=0.0f;

	int numTraces=0;
	int traceIndex=0;
    int index=0;
	int numPoints=0;
	int errorCode=0;

	if(xScaleFactor <= 0)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) <<
        "Error: "<<EINVALID_X_SCALE_FACTOR <<": "<<
		getErrorMessage(EINVALID_X_SCALE_FACTOR) <<
		"LTKTraceGroup::scale()"<<endl;

		LTKReturnError(EINVALID_X_SCALE_FACTOR);
	}

	if(yScaleFactor <= 0)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) <<
        "Error: "<<EINVALID_Y_SCALE_FACTOR <<": "<<
		getErrorMessage(EINVALID_Y_SCALE_FACTOR) <<
		"LTKTraceGroup::scale()"<<endl;

		LTKReturnError(EINVALID_Y_SCALE_FACTOR);
	}

    errorCode = getBoundingBox(xMin, yMin, xMax, yMax);

	if( errorCode != SUCCESS )
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) <<
        "Error: LTKTraceGroup::scale()"<<endl;

		LTKReturnError(errorCode);
	}

	switch ( cornerToPreserve )
	{

		case XMIN_YMIN:

			xToPreserve = xMin;
			yToPreserve = yMin;

			break;

		case XMIN_YMAX:

			xToPreserve=xMin;
			yToPreserve=yMax;

			break;


		case XMAX_YMIN:

			xToPreserve=xMax;
			yToPreserve=yMin;

			break;


		case XMAX_YMAX:
			xToPreserve=xMax;
			yToPreserve=yMax;

			break;


		default: break;//define an exception enum input validation

	}

    numTraces = getNumTraces();
    for(traceIndex=0; traceIndex < numTraces; ++traceIndex)
    {

		getTraceAt(traceIndex, trace);

		floatVector xVec;
		
		//no error handling required as the bounding box is found
        trace.getChannelValues(X_CHANNEL_NAME, xVec);

       
		floatVector yVec;
		
        trace.getChannelValues(Y_CHANNEL_NAME, yVec);

        
		numPoints = xVec.size();

		for(index=0; index < numPoints; ++index)
		{
			//the additive term is to translate back the scaled tracegroup
			//so that the corner asked for is preserved
			x= ( (xVec.at(index)*xScaleFactor)/m_xScaleFactor) +
        		 (xToPreserve*(1-(xScaleFactor/m_xScaleFactor)) );

			scaledXVec.push_back(x);

			//the additive term is to translate back the scaled tracegroup
			//so that the corner asked for is preserved
			y= ( (yVec.at(index)*yScaleFactor)/m_yScaleFactor) +
			     (yToPreserve*(1-(yScaleFactor/m_yScaleFactor)) );

            scaledYVec.push_back(y);
		}
		
		
		trace.reassignChannelValues(X_CHANNEL_NAME, scaledXVec);

		trace.reassignChannelValues(Y_CHANNEL_NAME, scaledYVec);

		scaledXVec.clear();

		scaledYVec.clear();

		scaledTracesVec.push_back(trace);

    }

	m_traceVector = scaledTracesVec;

	m_xScaleFactor = xScaleFactor;
	m_yScaleFactor = yScaleFactor;


	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Exit: LTKTraceGroup::scale()"<<endl;

	return SUCCESS;
}

/******************************************************************************
* AUTHOR		: Bharath A.
* DATE			: 03-SEP-2007
* NAME			: translateTo
* DESCRIPTION	: translates the tracegroup so that the "referenceCorner" is
				  moved to (x,y)
* ARGUMENTS		: x: x value of point to which referenceCorner has to be moved
*				  y: y value of point to which referenceCorner has to be moved
*				  referenceCorner - the reference corner in the tracegroup that
				  has to be moved to (x,y)
* RETURNS		: SUCCESS on successful translation operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*******************************************************************************/
int LTKTraceGroup::translateTo(float x,float y,TGCORNER referenceCorner)
{

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Enter: LTKTraceGroup::translateTo()"<<endl;

	LTKTrace trace;

	vector<LTKTrace> translatedTracesVec;	// holds the translated traces

	floatVector translatedXVec;				//	translated x channel values of a trace
	floatVector translatedYVec;				//	translated y channel values of a trace

	float xValue, yValue;
	float xReference, yReference;
	float xMin=0.0f;
	float yMin=0.0f;
	float xMax=0.0f;
	float yMax=0.0f;

	int errorCode;
	int traceIndex, index;
	int numPoints;


	if((errorCode = getBoundingBox(xMin,yMin,xMax,yMax))!=SUCCESS)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) <<
           "Error: LTKTraceGroup::translateTo()"<<endl;

		LTKReturnError(errorCode);
	}

	switch(referenceCorner)
	{

		case XMIN_YMIN:

			xReference=xMin;
			yReference=yMin;
			break;

		case XMIN_YMAX:

			xReference=xMin;
			yReference=yMax;

			break;


		case XMAX_YMIN:

			xReference=xMax;
			yReference=yMin;

			break;


		case XMAX_YMAX:
			xReference=xMax;
			yReference=yMax;

			break;


		default: break;//define an exception

	}

    int numTraces = getNumTraces();
    for(traceIndex=0; traceIndex < numTraces; ++traceIndex)
    {
        getTraceAt(traceIndex, trace);
		
		floatVector xVec;
		
		//no error handling required as the bounding box is found
        trace.getChannelValues(X_CHANNEL_NAME, xVec);

        
		floatVector yVec;
		
        trace.getChannelValues(Y_CHANNEL_NAME, yVec);

        
		numPoints = xVec.size();

		for(index=0; index < numPoints; index++)
		{

			xValue=xVec.at(index)+(x-xReference);
			translatedXVec.push_back(xValue);

			yValue=yVec.at(index)+(y-yReference);
			translatedYVec.push_back(yValue);

		}

		trace.reassignChannelValues(X_CHANNEL_NAME,translatedXVec);

		trace.reassignChannelValues(Y_CHANNEL_NAME,translatedYVec);

		translatedXVec.clear();

		translatedYVec.clear();

		translatedTracesVec.push_back(trace);

    }

	m_traceVector=translatedTracesVec;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Exit: LTKTraceGroup::translateTo()"<<endl;

	return SUCCESS;

}

/**********************************************************************************
* AUTHOR		: Bharath A.
* DATE			: 03-SEP-2007
* NAME			: affineTransform
* DESCRIPTION	: scales the tracegroup according to the x and y scale factors.
*				  After scaling, the "referenceCorner" of the tracegroup is translated to
*				  (translateToX,translateToY)
* ARGUMENTS		: xScaleFactor - factor by which x dimension has to be scaled
*				  yScaleFactor - factor by which y dimension has to be scaled
*				  referenceCorner - corner to be retained after scaling and moved to (translateToX,translateToY)
* RETURNS		: SUCCESS on successful scale operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
int LTKTraceGroup::affineTransform(float xScaleFactor,float yScaleFactor,
								   float translateToX,float translateToY,
								   TGCORNER referenceCorner)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Enter: LTKTraceGroup::translateTo()"<<endl;

	LTKTrace trace;

	vector<LTKTrace> scaledTracesVec;	// holds the scaled traces

	floatVector scaledXVec;				//	scaled x channel values of a trace
	floatVector scaledYVec;				//	scaled y channel values of a trace

	float x, y;
	float xReference, yReference;
	float xMin=0.0f;
	float yMin=0.0f;
	float xMax=0.0f;
	float yMax=0.0f;

	int traceIndex, index;
	int errorCode;
	int numPoints;

	if(xScaleFactor <= 0)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) <<
        "Error: "<<EINVALID_X_SCALE_FACTOR <<": "<<
		getErrorMessage(EINVALID_X_SCALE_FACTOR) <<
		"LTKTraceGroup::scale()"<<endl;

		LTKReturnError(EINVALID_X_SCALE_FACTOR);
	}

	if(yScaleFactor <= 0)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) <<
        "Error: "<<EINVALID_X_SCALE_FACTOR <<": "<<
		getErrorMessage(EINVALID_X_SCALE_FACTOR) <<
		"LTKTraceGroup::scale()"<<endl;

		LTKReturnError(EINVALID_Y_SCALE_FACTOR);
	}

	if((errorCode = getBoundingBox(xMin,yMin,xMax,yMax))!=SUCCESS)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) <<
        "Error: LTKTraceGroup::affineTransform()"<<endl;

		LTKReturnError(errorCode);
	}

	switch(referenceCorner)
	{

		case XMIN_YMIN:

			xReference=xMin;
			yReference=yMin;
			break;

		case XMIN_YMAX:

			xReference=xMin;
			yReference=yMax;

			break;

		case XMAX_YMIN:

			xReference=xMax;
			yReference=yMin;

			break;

		case XMAX_YMAX:
			xReference=xMax;
			yReference=yMax;

			break;

		default: break;//define an exception

	}

    int numTraces = m_traceVector.size();
    for(traceIndex=0; traceIndex < numTraces; ++traceIndex)
    {
        getTraceAt(traceIndex, trace);

		floatVector xVec;
		
		//no error handling required as the bounding box is found
        trace.getChannelValues(X_CHANNEL_NAME, xVec);

        
		floatVector yVec;
		
		
        trace.getChannelValues(Y_CHANNEL_NAME, yVec);
		

		numPoints = xVec.size();
		
		for(index=0; index < numPoints; index++)
		{
			//the additive term is to translate back the scaled tracegroup
			//so that the corner asked for is preserved at the destination
			//(translateToX,translateToY)
			x = ( (xVec.at(index) * xScaleFactor)/m_xScaleFactor) +
			     (translateToX - (xReference*(xScaleFactor/m_xScaleFactor)) );

			scaledXVec.push_back(x);

			//the additive term is to translate back the scaled tracegroup
			//so that the corner asked for is preserved at the destination
			//(translateToX,translateToY)
			y= ( (yVec.at(index) * yScaleFactor)/m_yScaleFactor) +
			     (translateToY - (yReference*(yScaleFactor/m_yScaleFactor)));

			scaledYVec.push_back(y);

		}

		trace.reassignChannelValues(X_CHANNEL_NAME,scaledXVec);

		trace.reassignChannelValues(Y_CHANNEL_NAME,scaledYVec);

		scaledXVec.clear();

		scaledYVec.clear();

		scaledTracesVec.push_back(trace);

    }


	m_traceVector = scaledTracesVec;

	m_xScaleFactor = xScaleFactor;
	m_yScaleFactor = yScaleFactor;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Enter: LTKTraceGroup::affineTransform()"<<endl;

	return SUCCESS;
}


/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: ~LTKTraceGroup
* DESCRIPTION	: destructor
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

LTKTraceGroup::~LTKTraceGroup()
{

}
/**********************************************************************************
 * AUTHOR               : Saravanan. R
 * DATE                 : 30-01-2007
 * NAME          : checkEmptyTraces
 * DESCRIPTION   : This method checks for empty traces
 * ARGUMENTS     : inTraceGroup : LTKTraceGroup :
 * RETURNS       : 1 if it contains empty trace group, 0 otherwise
 * NOTES         :
 * CHANGE HISTROY
 * Author            Date                Description
 *************************************************************************************/
bool LTKTraceGroup::containsAnyEmptyTrace() const
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Entering LTKTraceGroup::containsAnyEmptyTrace()"  <<endl;


    const vector<LTKTrace>& tracesVec = getAllTraces();  //traces in trace group

    int numTraces = tracesVec.size();

    if(numTraces == 0)
    {
        LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Numer of traces in the tracegroup=0 "
            " LTKTraceGroup::containsAnyEmptyTrace()"  <<endl;
        return true;
    }

    for(int traceIndex=0; traceIndex < numTraces; ++traceIndex)
    {
        const LTKTrace& trace = tracesVec.at(traceIndex);

        if(trace.isEmpty())
        {
            LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
                "Trace is Empty "
                " LTKTraceGroup::containsAnyEmptyTrace()"  <<endl;
            return true;
        }
    }

    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Exiting LTKTraceGroup::containsAnyEmptyTrace()"  <<endl;
    return false;
}
