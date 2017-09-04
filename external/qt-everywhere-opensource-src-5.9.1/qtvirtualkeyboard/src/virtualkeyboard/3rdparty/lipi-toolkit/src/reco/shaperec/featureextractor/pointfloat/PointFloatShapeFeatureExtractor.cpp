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
 * $LastChangedDate: 2009-07-01 20:12:22 +0530 (Wed, 01 Jul 2009) $
 * $Revision: 784 $
 * $Author: mnab $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Implementation for PointFloat Shape FeatureExtractor module
 *
 * CONTENTS: 
 *
 * AUTHOR:     Saravanan R.
 *
 * DATE:       March 15, 2007
 * CHANGE HISTORY:
 * Author       Date            Description of change
 ************************************************************************/

#include "PointFloatShapeFeatureExtractor.h"
#include "PointFloatShapeFeature.h"
#include "LTKTraceGroup.h"
#include "LTKTrace.h"
#include "LTKChannel.h"
#include "LTKTraceFormat.h"
#include "LTKConfigFileReader.h"
#include "LTKMacros.h"
#include "LTKPreprocDefaults.h"
#include "LTKException.h"
#include "LTKErrors.h"
#include "LTKLoggerUtil.h"


/**********************************************************************************
 * AUTHOR		: Naveen Sundar G
 * DATE			: 1-Oct-2007
 * NAME			: PointFloatShapeFeatureExtractor
 * DESCRIPTION	: parameterized constructor
 * ARGUMENTS		: 
 * RETURNS		: 
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date			
 *************************************************************************************/
PointFloatShapeFeatureExtractor::PointFloatShapeFeatureExtractor(
                                            const LTKControlInfo& controlInfo)
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
            "Error: PointFloatShapeFeatureExtractor::PointFloatShapeFeatureExtractor()"
            << endl;

		throw LTKException(returnVal);
	}

    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "PointFloatShapeFeatureExtractor::PointFloatShapeFeatureExtractor()" << endl;
}

/**********************************************************************************
 * AUTHOR		: Naveen Sundar G
 * DATE			: 1-Oct-2007
 * NAME			: readConfig
 * DESCRIPTION	: read the config values from cfg file and set member variables
 * ARGUMENTS		: 
 * RETURNS		: 
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date			
 *************************************************************************************/

int PointFloatShapeFeatureExtractor::readConfig(const string& cfgFilePath)
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "PointFloatShapeFeatureExtractor::readConfig()" << endl;
    
	LTKConfigFileReader* configurableProperties = NULL;
	string tempStringVar = "";

	try
	{
		configurableProperties = new LTKConfigFileReader(cfgFilePath);
	}

	catch(LTKException e)
	{
		delete configurableProperties;

        int eCode = e.getErrorCode();

        LOG( LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << eCode << 
            " : " << getErrorMessage(eCode) <<
            " PointFloatShapeFeatureExtractor::readConfig" <<endl;
        
		LTKReturnError(eCode);
	}
    
	delete configurableProperties;

    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "PointFloatShapeFeatureExtractor::readConfig()" << endl;
	return SUCCESS;

}

/**************************************************************************
 * AUTHOR		: Naveen Sundar G
 * DATE			: 
 * NAME			: 
 * DESCRIPTION	: 
 * ARGUMENTS	: 
 * RETURNS		: 
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date			
 ****************************************************************************/
 
int PointFloatShapeFeatureExtractor::extractFeatures(const LTKTraceGroup& inTraceGroup,
                                   vector<LTKShapeFeaturePtr>& outFeatureVec)
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "PointFloatShapeFeatureExtractor::extractFeatures()" << endl;
    
    PointFloatShapeFeature *featurePtr = NULL;
	float x,y,deltax;
	int numPoints=0;						// number of pts
	int count=0;
	int currentStrokeSize;
	float sintheta, costheta,sqsum;
	int i;

    int numberOfTraces = inTraceGroup.getNumTraces();

    if (numberOfTraces == 0 )
    {
        LOG( LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << 
            EEMPTY_TRACE_GROUP << " : " << getErrorMessage(EEMPTY_TRACE_GROUP)<<
            " PointFloatShapeFeatureExtractor::extractFeatures" <<endl;
        
        LTKReturnError(EEMPTY_TRACE_GROUP);
    }
    
	LTKTraceVector allTraces = inTraceGroup.getAllTraces();
	LTKTraceVector::iterator traceIter = allTraces.begin();
	LTKTraceVector::iterator traceEnd = allTraces.end();


	//***CONCATENTATING THE STROKES***
	for (; traceIter != traceEnd ; ++traceIter)
	{
		floatVector tempxVec, tempyVec;
		
		(*traceIter).getChannelValues("X", tempxVec);

		(*traceIter).getChannelValues("Y", tempyVec);

		// Number of points in the stroke
		numPoints = numPoints + tempxVec.size(); 
	}	

	//***THE CONCATENATED FULL STROKE***
	floatVector xVec(numPoints);
	floatVector yVec(numPoints);	


	traceIter = allTraces.begin();
	traceEnd  = allTraces.end();

	boolVector penUp;
	// Add the penUp here	
	for (; traceIter != traceEnd ; ++traceIter)
	{
		floatVector tempxVec, tempyVec;
		
		(*traceIter).getChannelValues("X", tempxVec);

		(*traceIter).getChannelValues("Y", tempyVec);

		currentStrokeSize = tempxVec.size();

        if (currentStrokeSize == 0)
        {
            LOG( LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << 
            EEMPTY_TRACE << " : " << getErrorMessage(EEMPTY_TRACE) <<
            " PointFloatShapeFeatureExtractor::extractFeatures" <<endl;
            
            LTKReturnError(EEMPTY_TRACE);
        }
        
		for( int point=0; point < currentStrokeSize ; point++ )
		{
			xVec[count] = tempxVec[point];
			yVec[count] = tempyVec[point];
			count++;
            
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
	//***CONCATENTATING THE STROKES***

	vector<float> theta(numPoints);
	vector<float> delta_x(numPoints-1);
	vector<float> delta_y(numPoints-1);

	for(i=0; i<numPoints-1; ++i)
	{  
		delta_x[i]=xVec[i+1]-xVec[i];
		delta_y[i]=yVec[i+1]-yVec[i];

	}

	//Add the controlInfo here
	sqsum = sqrt( pow(xVec[0],2)+ pow(yVec[0],2))+ EPS;
    
	sintheta = (1+yVec[0]/sqsum)*PREPROC_DEF_NORMALIZEDSIZE/2;
    
	costheta = (1+xVec[0]/sqsum)*PREPROC_DEF_NORMALIZEDSIZE/2;

    featurePtr = new PointFloatShapeFeature(xVec[0],
                                             yVec[0],
                                             sintheta,
                                             costheta,
                                             penUp[0]);

	outFeatureVec.push_back(LTKShapeFeaturePtr(featurePtr));
	featurePtr = NULL;

    
	for( i=1; i<numPoints; ++i)
	{  

		//Add the controlInfo here

		sqsum = sqrt(pow(delta_x[i-1],2) + pow(delta_y[i-1],2))+EPS;
		sintheta = (1+delta_y[i-1]/sqsum)*PREPROC_DEF_NORMALIZEDSIZE/2;
		costheta = (1+delta_x[i-1]/sqsum)*PREPROC_DEF_NORMALIZEDSIZE/2;

        featurePtr = new PointFloatShapeFeature(xVec[i],
                                               yVec[i],
                                               sintheta,
                                               costheta,
                                               penUp[i]);
		//***POPULATING THE FEATURE VECTOR***
		outFeatureVec.push_back(LTKShapeFeaturePtr(featurePtr));
		featurePtr = NULL;
    
	}

    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "PointFloatShapeFeatureExtractor::extractFeatures()" << endl;
    
	return SUCCESS;
}

/**************************************************************************
 * AUTHOR		: Naveen Sundar G
 * DATE			: 
 * NAME			: 
 * DESCRIPTION	: 
 * ARGUMENTS	: 
 * RETURNS		: 
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date			
 ****************************************************************************/
LTKShapeFeaturePtr PointFloatShapeFeatureExtractor::getShapeFeatureInstance()
{
	LTKShapeFeaturePtr tempPtr(new PointFloatShapeFeature);	
	return tempPtr;
}


/******************************************************************************
 * AUTHOR		: Tarun Madan
 * DATE			: Aug-07-2007
 * NAME			: convertToTraceGroup
 * DESCRIPTION	: 
 * ARGUMENTS		: 
 * RETURNS		:
 * NOTES			: 
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int PointFloatShapeFeatureExtractor::convertFeatVecToTraceGroup(
                                 const vector<LTKShapeFeaturePtr>& shapeFeature, 
                                 LTKTraceGroup& outTraceGroup)
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "PointFloatShapeFeatureExtractor::convertFeatVecToTraceGroup()" << endl;
    
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
	int featureVectorSize = shapeFeature.size();

	for(int count=0; count < featureVectorSize; count++)
	{
		float Xpoint, Ypoint;
		bool penUp;

		PointFloatShapeFeature* ptr = (PointFloatShapeFeature*)(shapeFeature[count].operator ->());
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

    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "PointFloatShapeFeatureExtractor::convertFeatVecToTraceGroup()" << endl;
    
	return SUCCESS;
}
