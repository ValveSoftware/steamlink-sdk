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
 * $LastChangedDate: 2008-02-20 10:03:51 +0530 (Wed, 20 Feb 2008) $
 * $Revision: 423 $
 * $Author: sharmnid $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Implementation of RunShaperec tool
 *
 * CONTENTS:
 *			extractFeatures
 *			getShapeFeatureInstance
 *			clearFeatureVector
 *			computeDerivativeDenominator
 *			computeDerivative
 *
 * AUTHOR:     Naveen Sundar G.
 *
 * DATE:       August 30, 2005
 * CHANGE HISTORY:
 * Author       Date            Description of change
 ************************************************************************/


#include "L7ShapeFeatureExtractor.h"
#include "L7ShapeFeature.h"
#include "LTKTraceGroup.h"
#include "LTKTrace.h"
#include "LTKChannel.h"
#include "LTKTraceFormat.h"
#include "LTKConfigFileReader.h"
#include "LTKMacros.h"
#include "LTKException.h"
#include "LTKErrors.h"
#include "LTKLoggerUtil.h"

/******************************************************************************
* AUTHOR		: Dinesh M
* DATE			: 1-Oct-2007
* NAME			: L7ShapeFeatureExtractor
* DESCRIPTION	: parameterized constructor
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date
*******************************************************************************/
L7ShapeFeatureExtractor::L7ShapeFeatureExtractor(const LTKControlInfo& controlInfo):
m_radius(FEATEXTR_L7_DEF_RADIUS)
{    
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
        throw LTKException(EINVALID_PROJECT_NAME);
    }

	int returnVal = readConfig(cfgFilePath);
    
    if (returnVal != SUCCESS)
	{
	    LOG(LTKLogger::LTK_LOGLEVEL_ERR) << 
            "Error: L7ShapeFeatureExtractor::L7ShapeFeatureExtractor()" <<endl;
        
		throw LTKException(returnVal);
	}

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "L7ShapeFeatureExtractor::L7ShapeFeatureExtractor()" << endl;
    
}

/**********************************************************************************
* AUTHOR		: Dinesh M
* DATE			: 1-Oct-2007
* NAME			: readConfig
* DESCRIPTION	: read the config values from cfg file and set member variables
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date
*************************************************************************************/

int L7ShapeFeatureExtractor::readConfig(const string& cfgFilePath)
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "L7ShapeFeatureExtractor::readConfig()" << endl;
    
	LTKConfigFileReader* configurableProperties = NULL;
    string tempStringVar = "";

	try
    {
        configurableProperties = new LTKConfigFileReader(cfgFilePath);
        
		int errorCode = configurableProperties->getConfigValue(L7RADIUS, tempStringVar);
		
		if( errorCode == SUCCESS)
        {
            if (setRadius(atoi((tempStringVar).c_str())) != SUCCESS)
            {
                LOG( LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << 
                    ECONFIG_FILE_RANGE << " : " << 
                    getErrorMessage(ECONFIG_FILE_RANGE) <<
                    " L7ShapeFeatureExtractor::readConfig" <<endl;
                
                throw LTKException(ECONFIG_FILE_RANGE);
            }
        }
	}

	catch(LTKException e)
    {
        delete configurableProperties;

        int eCode = e.getErrorCode();
        
        LOG( LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << eCode << 
            " : " << getErrorMessage(eCode) <<
            " L7ShapeFeatureExtractor::readConfig" <<endl;
        
        LTKReturnError(eCode);
    }
	delete configurableProperties;

    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "L7ShapeFeatureExtractor::readConfig()" << endl;
    
	return SUCCESS;

}

/**********************************************************************************
* AUTHOR		: Dinesh M
* DATE			: 1-Oct-2007
* NAME			: setRadius
* DESCRIPTION	: set the radius(the size of window) to compute L7 features
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date
*************************************************************************************/
int L7ShapeFeatureExtractor::setRadius(int radius)
{
    int returnVal = FAILURE;

    if ( radius > 0 )
    {
	    m_radius = radius;
        returnVal = SUCCESS;
    }

	return returnVal;
}

/**********************************************************************************
* AUTHOR		: Nidhi Sharma
* DATE			: 15-Feb-2008
* NAME			: getRadius
* DESCRIPTION	: 
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date
*************************************************************************************/
int L7ShapeFeatureExtractor::getRadius()
{
	return m_radius;
}


/**********************************************************************************
* AUTHOR        : Naveen Sundar G.
* DATE          : 10-AUGUST-2007
* NAME          : extractFeatures
* DESCRIPTION   : Extracts L7 features from a trace group
* ARGUMENTS     : The trace group from which features have to be extracted
* RETURNS       : vector of L7ShapeFeature objects
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
int L7ShapeFeatureExtractor::extractFeatures(const LTKTraceGroup& inTraceGroup, 
                                      vector<LTKShapeFeaturePtr>& outFeatureVec)
{

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "L7ShapeFeatureExtractor::extractFeatures()" << endl;

    L7ShapeFeature* featurePtr = NULL;  
    float delta;
    int currentStrokeSize;
    int numPoints;

    int numberOfTraces = inTraceGroup.getNumTraces();

    if (numberOfTraces == 0 )
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error :" <<
            EEMPTY_TRACE_GROUP << " : " << getErrorMessage(EEMPTY_TRACE_GROUP) <<
        "L7ShapeFeatureExtractor::extractFeatures()" << endl;

        LTKReturnError(EEMPTY_TRACE_GROUP);
    }
    
    
    LTKTraceVector allTraces = inTraceGroup.getAllTraces();
    LTKTraceVector::iterator traceIter = allTraces.begin();
    LTKTraceVector::iterator traceEnd = allTraces.end();

    //the feature vector
    vector<float> xVec;
    vector<float> yVec;
    vector<bool> penUp;
    
    //concatentating the strokes
    for (; traceIter != traceEnd ; ++traceIter)
    {
    	floatVector tempxVec, tempyVec;

    	(*traceIter).getChannelValues("X", tempxVec);

    	(*traceIter).getChannelValues("Y", tempyVec);

    	currentStrokeSize = tempxVec.size();

        if (currentStrokeSize == 0)
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error :" <<
                EEMPTY_TRACE<< " : " << getErrorMessage(EEMPTY_TRACE) <<
                "L7ShapeFeatureExtractor::extractFeatures()" << endl;

            LTKReturnError(EEMPTY_TRACE);
        }

    	for(int point=0;point<currentStrokeSize;point++)
    	{
    		xVec.push_back(tempxVec[point]);
    		yVec.push_back(tempyVec[point]);
            
    		if(point == currentStrokeSize - 1 )
            {      
    			penUp.push_back(true); 
            }
    		else
            {      
    			penUp.push_back(false);
            }
    	}
    }
		//concatentating the strokes

	numPoints=xVec.size();

    vector<float> normfirstderv_x(numPoints);	// Array to store the first normalized derivative w.r.t x
	vector<float> normfirstderv_y(numPoints);	// Array to store the first normalized derivative w.r.t y
	vector<float> normsecondderv_x(numPoints);	// Array to store the second normalized derivative w.r.t x
	vector<float> normsecondderv_y(numPoints);	// Array to store the second normalized derivative w.r.t y
	vector<float> curvature(numPoints);			// Array to store the curvature

	computeDerivative(xVec,yVec,normfirstderv_x,normfirstderv_y,m_radius);
    
	computeDerivative(normfirstderv_x,normfirstderv_y,normsecondderv_x,normsecondderv_y,m_radius);

	for(int i=0; i<numPoints; ++i)
	{
		//computing the curvature
		delta= sqrt(pow(pow(normfirstderv_x[i],2)+pow(normfirstderv_y[i],2),3));
		curvature[i]= ((normfirstderv_x[i]*normsecondderv_y[i]) - (normsecondderv_x[i]*normfirstderv_y[i]))/(delta+EPS);

        featurePtr = new L7ShapeFeature(xVec[i], yVec[i],
		                             normfirstderv_x[i], normfirstderv_y[i],
		                             normsecondderv_x[i], normsecondderv_y[i],
		                             curvature[i], penUp[i]);
		//populating the feature vector
		outFeatureVec.push_back(LTKShapeFeaturePtr(featurePtr));
		featurePtr = NULL;
	}

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "L7ShapeFeatureExtractor::extractFeatures()" << endl;
    
	return SUCCESS;
}

/**********************************************************************************
* AUTHOR        : Naveen Sundar G.
* DATE          : 10-AUGUST-2007
* NAME          : getShapeFeatureInstance
* DESCRIPTION   : Returns an L7ShapeFeature instance
* ARGUMENTS     :
* RETURNS       : An L7ShapeFeature instance
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
LTKShapeFeaturePtr L7ShapeFeatureExtractor::getShapeFeatureInstance()
{
	LTKShapeFeaturePtr tempPtr(new L7ShapeFeature);
	return tempPtr;
}


/******************************************************************************
* AUTHOR		: Naveen Sundar G.
* DATE			: Sept-10-2007
* NAME			: convertFeatVecToTraceGroup
* DESCRIPTION	:
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
******************************************************************************/
int L7ShapeFeatureExtractor::convertFeatVecToTraceGroup(
                                 const vector<LTKShapeFeaturePtr>& shapeFeature,
                                 LTKTraceGroup& outTraceGroup)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "L7ShapeFeatureExtractor::convertFeatVecToTraceGroup()" << endl;
    
	vector<LTKChannel> channels;				//	channels of a trace

	LTKChannel xChannel("X", DT_INT, true);	//	x-coordinate channel of the trace
	LTKChannel yChannel("Y", DT_INT, true);	//	y-coordinate channel of the trace

	//initializing the channels of the trace
	channels.push_back(xChannel);
	channels.push_back(yChannel);

	//	composing the trace format object
	LTKTraceFormat traceFormat(channels);

	vector<float> point;				//	a point of a trace

	LTKTrace trace(traceFormat);

	for(int count=0;count<(int)shapeFeature.size();count++)
	{
		float Xpoint, Ypoint;
		bool penUp;

		L7ShapeFeature* ptr = (L7ShapeFeature*)(shapeFeature[count].operator ->());
		Xpoint = ptr->getX();
		Ypoint = ptr->getY();
		penUp = ptr->isPenUp();



		point.push_back(Xpoint);
		point.push_back(Ypoint);

		trace.addPoint(point);
		point.clear();


		if(penUp == true)	// end of a trace, clearing the trace now
		{
			outTraceGroup.addTrace(trace);
			trace.emptyTrace();
			LTKTrace tempTrace(traceFormat);
			trace = tempTrace;
		}
	}

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "L7ShapeFeatureExtractor::convertFeatVecToTraceGroup()" << endl;
	return SUCCESS;

}

/**********************************************************************************
* AUTHOR        : Naveen Sundar G.
* DATE          : 10-AUGUST-2007
* NAME          : computeDerivativeDenominator
* DESCRIPTION   : Computes the denominator to be used in the derivative computation
* ARGUMENTS     : The index used in derivative computation
* RETURNS       : The denominator
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
int L7ShapeFeatureExtractor::computeDerivativeDenominator(int index)
{
	int denominator=0;

	for (int i=1;i<=index;i++)
	{

		denominator=denominator+i*i;
	}
	return 2*denominator;
}

/**********************************************************************************
* AUTHOR        : Naveen Sundar G.
* DATE          : 10-AUGUST-2007
* NAME          : computeDerivative
* DESCRIPTION   : Computes the derivative
* ARGUMENTS     : The input and output vectors. The output vectors are the derivatives of the input vectors
* RETURNS       :
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
void L7ShapeFeatureExtractor::computeDerivative(const vector<float>& xVec,
                                                   const vector<float>& yVec,
                                                   vector<float>& dx,
                                                   vector<float>& dy,
                                                   int index)
{
	int size = xVec.size();
	int denominator = computeDerivativeDenominator(index);
	float x,y,diffx,diffy,delta;
	float firstderv_x,firstderv_y;
	int i,j;
    
	if(index<size-index)
	{
		for(i=index; i<size-index; ++i)
		{
			x= xVec[i];
			y= yVec[i];
     		diffx=0;
			diffy=0;

			for( j=1;j<=index;j++)
			{
				diffx = diffx + j*(xVec[i+j]-xVec[i-j]) ;
				diffy = diffy + j*(yVec[i+j]-yVec[i-j]) ;
			}

			firstderv_x = diffx/denominator;
			firstderv_y = diffy/denominator;
			delta		= sqrt(pow(firstderv_x,2)+pow(firstderv_y,2));
			if(delta!=0)
			{
				dx[i]   = firstderv_x/(delta);
		 		dy[i]   = firstderv_y/(delta);
			}
			else
			{
				dx[i]   = 0;
		 		dy[i]   = 0;
			}


		}

		for(i=0;i<index;i++)
		{
			x= xVec[i];
			y= yVec[i];
     		diffx=0;
			diffy=0;

			for( j=1;j<=index;j++)
			{
				diffx = diffx + j*(xVec[i+j]-x) ;
				diffy = diffy + j*(yVec[i+j]-y) ;
			}
			firstderv_x = diffx/denominator;
			firstderv_y = diffy/denominator;
			delta		= sqrt(pow(firstderv_x,2)+pow(firstderv_y,2));
			if(delta!=0)
			{
				dx[i]   = firstderv_x/(delta);
		 		dy[i]   = firstderv_y/(delta);
			}
			else
			{
				dx[i]   = 0;
		 		dy[i]   = 0;
			}

		}

		for(i=size-index;i<=size-1;i++)
		{
			x= xVec[i];
			y= yVec[i];
     		diffx=0;
			diffy=0;

			for( j=1;j<=index;j++)
			{
				diffx = diffx + j*(x-xVec[i-j]) ;
				diffy = diffy + j*(y-yVec[i-j]) ;
			}
			firstderv_x = diffx/denominator;
			firstderv_y = diffy/denominator;
			delta		= sqrt(pow(firstderv_x,2)+pow(firstderv_y,2));
			if(delta!=0)
			{
				dx[i]   = firstderv_x/(delta);
		 		dy[i]   = firstderv_y/(delta);
			}
			else
			{
				dx[i]   = 0;
		 		dy[i]   = 0;
			}
		}
	}
	if(index>size-index)
	{
		for(i=0; i<size; ++i)
		{
			x= xVec[i];
			y= yVec[i];
     		diffx=0;
			diffy=0;
			if((0<i+j)&(i+j<size))
			{
				for( j=1;j<=index;j++)
				{
					diffx = diffx + j*(xVec[i+j]-x) ;
					diffy = diffy + j*(yVec[i+j]-y) ;
				}
			}
			else
			{
				for(j=1;j<=index;j++)
				{
					diffx = diffx + j*(x-xVec[i-j]) ;
					diffy = diffy + j*(y-yVec[i-j]) ;
				}
			}

			firstderv_x = diffx/denominator;
			firstderv_y = diffy/denominator;
			delta		= sqrt(pow(firstderv_x,2)+pow(firstderv_y,2));
			if(delta!=0)
			{
				dx[i]   = firstderv_x/(delta);
		 		dy[i]   = firstderv_y/(delta);
			}
			else
			{
				dx[i]   = 0;
		 		dy[i]   = 0;
			}

		}
	}
}

