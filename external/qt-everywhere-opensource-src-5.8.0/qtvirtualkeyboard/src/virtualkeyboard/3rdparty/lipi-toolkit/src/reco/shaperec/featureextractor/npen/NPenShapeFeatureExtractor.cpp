/*****************************************************************************************
* Copyright (c) 2006 Hewlett-Packard Development Company, L.P.
* Permission is hereby granted, free of charge, to any person obtaining a copy of
* this software and associated documentation files (the "Software"), to deal in
* the Software without restriction, including without limitation the rights to use,
* copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
* Software, and to permit persons to whom the Software is furnished to do so,
* subject to the following conditions:
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
 *
 * AUTHOR:     Bharath A
 *
 * DATE:       June 11, 2008
 * CHANGE HISTORY:
 * Author       Date            Description of change
 ************************************************************************/


#include "NPenShapeFeatureExtractor.h"
#include "NPenShapeFeature.h"
#include "LTKTraceGroup.h"
#include "LTKTrace.h"
#include "LTKChannel.h"
#include "LTKTraceFormat.h"
#include "LTKConfigFileReader.h"
#include "LTKMacros.h"
#include "LTKException.h"
#include "LTKErrors.h"
#include "LTKLoggerUtil.h"
#include "LTKPreprocDefaults.h"

/******************************************************************************
* AUTHOR		: Dinesh M
* DATE			: 1-Oct-2007
* NAME			: NPenShapeFeatureExtractor
* DESCRIPTION	: parameterized constructor
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date
*******************************************************************************/
NPenShapeFeatureExtractor::NPenShapeFeatureExtractor(const LTKControlInfo& controlInfo):
m_windowSize(FEATEXTR_NPEN_DEF_WINDOW_SIZE)
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
            "Error: NPenShapeFeatureExtractor::NPenShapeFeatureExtractor()" <<endl;
        
		throw LTKException(returnVal);
	}

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NPenShapeFeatureExtractor::NPenShapeFeatureExtractor()" << endl;
    
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

int NPenShapeFeatureExtractor::readConfig(const string& cfgFilePath)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NPenShapeFeatureExtractor::readConfig()" << endl;
    
	LTKConfigFileReader* configurableProperties = NULL;
    string tempStringVar = "";

	try
    {
        configurableProperties = new LTKConfigFileReader(cfgFilePath);
        
		int errorCode = configurableProperties->getConfigValue(NPEN_WINDOW_SIZE, tempStringVar);
		
		if( errorCode == SUCCESS)
        {
            if (setWindowSize(atoi((tempStringVar).c_str())) != SUCCESS)
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << 
                    ECONFIG_FILE_RANGE << " : " << 
                    getErrorMessage(ECONFIG_FILE_RANGE) <<
                    " NPenShapeFeatureExtractor::readConfig" <<endl;
                
                LTKReturnError(ECONFIG_FILE_RANGE);
            }
        }
	}

	catch(LTKException e)
    {
        delete configurableProperties;

        int eCode = e.getErrorCode();
        
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << eCode << 
            " : " << getErrorMessage(eCode) <<
            " NPenShapeFeatureExtractor::readConfig" <<endl;
        
        LTKReturnError(eCode);
    }
	delete configurableProperties;

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NPenShapeFeatureExtractor::readConfig()" << endl;
    
	return SUCCESS;

}

/**********************************************************************************
* AUTHOR		: Dinesh M
* DATE			: 1-Oct-2007
* NAME			: setRadius
* DESCRIPTION	: set the radius(the size of window) to compute NPen features
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date
*************************************************************************************/
int NPenShapeFeatureExtractor::setWindowSize(int windowSize)
{
    int returnVal = FAILURE;

    if ( windowSize > 0 && (windowSize%2 == 1))
    {
	    m_windowSize = windowSize;
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
int NPenShapeFeatureExtractor::getWindowSize()
{
	return m_windowSize;
}


/**********************************************************************************
* AUTHOR        : Bharath A
* DATE          : 12-Jun-2008
* NAME          : extractFeatures
* DESCRIPTION   : Extracts NPen features from a trace group
* ARGUMENTS     : The trace group from which features have to be extracted
* RETURNS       : vector of NPenShapeFeature objects
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
int NPenShapeFeatureExtractor::extractFeatures(const LTKTraceGroup& inTraceGroup, 
                                      vector<LTKShapeFeaturePtr>& outFeatureVec)
{

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NPenShapeFeatureExtractor::extractFeatures()" << endl;

    NPenShapeFeature* featurePtr = NULL;  

	vector<vector<float> > floatFeatureValues;
    


	int errorCode;

	if(inTraceGroup.getNumTraces() == 0)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
		        <<"Error: FeatureExtractor::findAllFeatures"<<endl;

		LTKReturnError(EEMPTY_TRACE_GROUP);
	}


	vector<vector<float> > concatenatedCoord;
	int currPenUpPointIndex = -1;
	vector<int> penUpPointsIndices;
	

	int halfWindowSize = m_windowSize/2;

	if(halfWindowSize==0)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
		        <<"Error: FeatureExtractor::findAllFeatures"<<endl;

		LTKReturnError(EINVALID_NUM_OF_POINTS);
	}


	for(int t=0;t<inTraceGroup.getNumTraces();++t)
	{

		LTKTrace eachTrace;
		inTraceGroup.getTraceAt(t,eachTrace);

		if(eachTrace.isEmpty())
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
						    <<"Error: FeatureExtractor::findAllFeatures"<<endl;

			LTKReturnError(EEMPTY_TRACE);

		}

		vector<float> xVec;
		vector<float> yVec;

		eachTrace.getChannelValues(X_CHANNEL_NAME,xVec);
		eachTrace.getChannelValues(Y_CHANNEL_NAME,yVec);

		if(t==0)
		{
			vector<float> firstPoint;
			firstPoint.push_back(xVec[0]);
			firstPoint.push_back(yVec[0]);

			concatenatedCoord.insert(concatenatedCoord.begin(),halfWindowSize,firstPoint);
		}

		for(int p=0;p<xVec.size();++p)
		{
			vector<float> point;
			point.push_back(xVec[p]);
			point.push_back(yVec[p]);

			concatenatedCoord.push_back(point);

		}

		currPenUpPointIndex += xVec.size();

		penUpPointsIndices.push_back(currPenUpPointIndex);

		if(t==(inTraceGroup.getNumTraces()-1))
		{
			vector<float> lastPoint;
			lastPoint.push_back(xVec[xVec.size()-1]);
			lastPoint.push_back(yVec[yVec.size()-1]);

			concatenatedCoord.insert(concatenatedCoord.end(),halfWindowSize,lastPoint);
		}

	}

	

	/*	0 - normalized x
		1 - normalized y
		2 - cos alpha
		3 - sin alpha
		4 - cos beta
		5 - sin beta
		6 - aspect
		7 - curliness
		8 - linearity
		9 - slope  
		10 - pen-up / pen-down stroke (0 for pen-down and 1 for pen-up)*/

	float deltaX=0;
	float deltaY=0;
	float hypotenuse=0;


	float cosalpha=0;
	float sinalpha=0;
	float cosbeta=0;
	float sinbeta=0;
	float ispenup=0;
	float aspect=0;
	float curliness=0;
	float linearity=0;
	float slope=0;


	float xMin,yMin,xMax,yMax; //for vicnity bounding box;
	float bbWidth,bbHeight;
	float maxOfWidthHeight;



	currPenUpPointIndex = 0;


	for(int f=halfWindowSize;f<(concatenatedCoord.size()-halfWindowSize);++f)
	{

		vector<float> eachPointFeature;

		eachPointFeature.push_back(concatenatedCoord[f][0]);  //x
		eachPointFeature.push_back(concatenatedCoord[f][1]);  //y

		deltaX = concatenatedCoord[f-1][0] - concatenatedCoord[f+1][0];
		deltaY = concatenatedCoord[f-1][1] - concatenatedCoord[f+1][1];

		hypotenuse = sqrt((deltaX*deltaX)+(deltaY*deltaY));

		if(hypotenuse < EPS)
		{
			cosalpha = 1;
			sinalpha = 0;
		}
		else
		{
			cosalpha = deltaX / hypotenuse;
			sinalpha = deltaY / hypotenuse;
		}

		eachPointFeature.push_back(cosalpha);
		eachPointFeature.push_back(sinalpha);

		eachPointFeature.push_back(cosbeta); //creating empty spaces for cosine and sine betas for future assignment
		eachPointFeature.push_back(sinbeta);

		vector<vector<float> > vicinity;

		float vicinityTrajLen = 0.0f;
		
		for(int v=f-halfWindowSize;v<=f+halfWindowSize;++v)
		{
			vicinity.push_back(concatenatedCoord[v]);

			if(v<(f+halfWindowSize))
			{
				vicinityTrajLen += (sqrt(((concatenatedCoord[v+1][1]-concatenatedCoord[v][1])*(concatenatedCoord[v+1][1]-concatenatedCoord[v][1]))+((concatenatedCoord[v+1][0]-concatenatedCoord[v][0])*(concatenatedCoord[v+1][0]-concatenatedCoord[v][0]))));
			}
		}

		findVicinityBoundingBox(vicinity,xMin,yMin,xMax,yMax);

		bbWidth = xMax - xMin;

		bbHeight = yMax - yMin;

		if(fabs(bbHeight+bbWidth)<EPS)
		{
			aspect = 0.0;
		}
		else
		{
			aspect = (bbHeight-bbWidth)/(bbHeight+bbWidth);
		}

		
		eachPointFeature.push_back(aspect);

		
		maxOfWidthHeight = ( bbWidth > bbHeight) ? bbWidth : bbHeight;

		if(fabs(maxOfWidthHeight) < EPS)
		{
			curliness = 0.0f;
		}
		else
		{
			curliness = (vicinityTrajLen / maxOfWidthHeight) - 2;
		}

		eachPointFeature.push_back(curliness);

		computeLinearityAndSlope(vicinity,linearity,slope);

		eachPointFeature.push_back(linearity);
		eachPointFeature.push_back(slope);

		if(penUpPointsIndices[currPenUpPointIndex] == (f-halfWindowSize))
		{
			ispenup = 1;
			++currPenUpPointIndex;
		}
		else
		{
			ispenup = 0;
		}
		eachPointFeature.push_back(ispenup); //currently assuming pen-up strokes are not resampled

		floatFeatureValues.push_back(eachPointFeature);

	}


		//duplicating first and last features
		vector<float> firstFeaturePoint = floatFeatureValues[0];

		floatFeatureValues.insert(floatFeatureValues.begin(),1,firstFeaturePoint);

		vector<float>  lastFeaturePoint = floatFeatureValues[floatFeatureValues.size()-1];

		floatFeatureValues.insert(floatFeatureValues.end(),1,lastFeaturePoint);


		for(int ff=1;ff<(floatFeatureValues.size()-1);++ff)
		{

			floatFeatureValues[ff][4] = (floatFeatureValues[ff-1][2]*floatFeatureValues[ff+1][2]) + (floatFeatureValues[ff-1][3]*floatFeatureValues[ff+1][3]);
			floatFeatureValues[ff][5] = (floatFeatureValues[ff-1][2]*floatFeatureValues[ff+1][3]) - (floatFeatureValues[ff-1][3]*floatFeatureValues[ff+1][2]);
			
		}

		//removing the extraneous feature points at the beginning and end
		floatFeatureValues.erase(floatFeatureValues.begin(),floatFeatureValues.begin()+1); 
		floatFeatureValues.pop_back();


		for(int a=0;a<floatFeatureValues.size();++a)
		{
				NPenShapeFeature* ptrFeature = new NPenShapeFeature();
				ptrFeature->setX(floatFeatureValues[a][0]);
				ptrFeature->setY(floatFeatureValues[a][1]);
				ptrFeature->setCosAlpha((1+floatFeatureValues[a][2])*((float)PREPROC_DEF_NORMALIZEDSIZE/2.0));
				ptrFeature->setSinAlpha((1+floatFeatureValues[a][3])*((float)PREPROC_DEF_NORMALIZEDSIZE/2.0));
				ptrFeature->setCosBeta((1+floatFeatureValues[a][4])*((float)PREPROC_DEF_NORMALIZEDSIZE/2.0));
				ptrFeature->setSinBeta((1+floatFeatureValues[a][5])*((float)PREPROC_DEF_NORMALIZEDSIZE/2.0));
				ptrFeature->setAspect(floatFeatureValues[a][6]);
				ptrFeature->setCurliness(floatFeatureValues[a][7]);
				ptrFeature->setLinearity(floatFeatureValues[a][8]);
				ptrFeature->setSlope((1+floatFeatureValues[a][9])*((float)PREPROC_DEF_NORMALIZEDSIZE/2.0));
				
				if(fabs(floatFeatureValues[a][10]-1.0f) < EPS)
				{
					ptrFeature->setPenUp(true);
				}
				else
				{
					ptrFeature->setPenUp(false);
				}

				outFeatureVec.push_back(LTKShapeFeaturePtr(ptrFeature));

				ptrFeature = NULL;
			
		}
		
	



    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NPenShapeFeatureExtractor::extractFeatures()" << endl;
    
	return SUCCESS;
}

/**********************************************************************************
* AUTHOR        : Bharath A
* DATE          : 12-Jun-2008
* NAME          : getShapeFeatureInstance
* DESCRIPTION   : Returns an NPenShapeFeature instance
* ARGUMENTS     :
* RETURNS       : An NPenShapeFeature instance
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
LTKShapeFeaturePtr NPenShapeFeatureExtractor::getShapeFeatureInstance()
{
	LTKShapeFeaturePtr tempPtr(new NPenShapeFeature);
	return tempPtr;
}


/******************************************************************************
* AUTHOR        : Bharath A
* DATE          : 12-Jun-2008
* NAME			: convertFeatVecToTraceGroup
* DESCRIPTION	:
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
******************************************************************************/
int NPenShapeFeatureExtractor::convertFeatVecToTraceGroup(
                                 const vector<LTKShapeFeaturePtr>& shapeFeature,
                                 LTKTraceGroup& outTraceGroup)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NPenShapeFeatureExtractor::convertFeatVecToTraceGroup()" << endl;
    
	vector<LTKChannel> channels;				//	channels of a trace

	LTKChannel xChannel("X", DT_FLOAT, true);	//	x-coordinate channel of the trace
	LTKChannel yChannel("Y", DT_FLOAT, true);	//	y-coordinate channel of the trace

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

		NPenShapeFeature* ptr = (NPenShapeFeature*)(shapeFeature[count].operator ->());
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
        "NPenShapeFeatureExtractor::convertFeatVecToTraceGroup()" << endl;
	return SUCCESS;

}


void NPenShapeFeatureExtractor::findVicinityBoundingBox(vector<vector<float> >& inputXYCoords, float& xMin, float& yMin, float& xMax, float& yMax)
	{
		xMin = FLT_MAX;
		yMin = FLT_MAX;

		xMax = -FLT_MAX;
		yMax = -FLT_MAX;
		
		for(int i=0;i<inputXYCoords.size();++i)
		{
			if(inputXYCoords[i][0] < xMin)
			{
				xMin = inputXYCoords[i][0];
			}

			if(inputXYCoords[i][0] > xMax)
			{
				xMax = inputXYCoords[i][0];
			}

			if(inputXYCoords[i][1] < yMin)
			{
				yMin = inputXYCoords[i][1];
			}

			if(inputXYCoords[i][1] > yMax)
			{
				yMax = inputXYCoords[i][1];
			}
		}

	}



int NPenShapeFeatureExtractor::computeLinearityAndSlope(const vector<vector<float> >& vicinity,float& linearity,float& slope)
	{
		if(vicinity.size()<3)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
		        <<"Error: FeatureExtractor::computeLinearity"<<endl;

			LTKReturnError(FAILURE);
		}

		float x1 = vicinity[0][0];
		float y1 = vicinity[0][1];

		float x2 = vicinity[vicinity.size()-1][0];
		float y2 = vicinity[vicinity.size()-1][1];

		float avgX = 0.0f;
		float avgY = 0.0f;

		float denominator = sqrt(((x2-x1)*(x2-x1))+((y2-y1)*(y2-y1)));

		/*if(denominator < EPS)
		{
			linearity = 0.0f;		
			slope = 1.0f;

			return SUCCESS;
		}*/

		if(denominator < EPS)
		{
			slope = 1.0f;


			//considering the case of loop where the end points are the same
			avgX = (x1+x2)/2.0f;
			avgY = (y1+y2)/2.0f;
		}
		else if(fabs(x2-x1) <  EPS)
		{
			slope = 0.0f;
		}
		else
		{
			slope = cos(atan((y2-y1)/(x2-x1)));
		}


		float x0 = 0.0f;
		float y0 = 0.0f;

		float distance = 0.0f;
		

		linearity = 0.0f;

		for(int v=1; v<(vicinity.size()-1); ++v)
		{

			if(vicinity[v].size() < 2)
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)
		        <<"Error: FeatureExtractor::computeLinearity"<<endl;

				LTKReturnError(FAILURE);
			}

			x0 = vicinity[v][0];
			y0 = vicinity[v][1];

			if(denominator < EPS)
			{
				distance = sqrt(((avgX-x0)*(avgX-x0))+((avgY-y0)*(avgY-y0)));
			}
			else
			{
				distance = fabs(((x2-x1)*(y1-y0))-((x1-x0)*(y2-y1))) / denominator;
			}
			
			linearity += (distance*distance);

		}

		linearity /= (vicinity.size()-2); // 2 to exclude the end points of vicinity while computing average squared distance

		return SUCCESS;
	}
