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
 * $LastChangedDate: 2008-07-18 15:00:39 +0530 (Fri, 18 Jul 2008) $
 * $Revision: 561 $
 * $Author: sharmnid $
 *
 ************************************************************************/
/************************************************************************
* FILE DESCR: Implementation for LTKShapeRecoUtil
*
* CONTENTS: 
*			 getAbsolutePath
* 			 isProjectDynamic
*			 initializePreprocessor
*			 deletePreprocessor
*			 unloadPreprocessor
*			 readInkFromFile
*			 checkEmptyTraces
*			 shapeFeatureVectorToFloatVector
*			 shapeFeatureVectorToIntVector
*			 
* AUTHOR:     Saravanan R.
*
* DATE:       January 23, 2004
* CHANGE HISTORY:
* Author       Date            Description of change
************************************************************************/

#ifdef _WIN32
#include "windows.h"
#endif

#include "LTKInc.h"
#include "LTKLoggerUtil.h"
#include "LTKTrace.h"
#include "LTKMacros.h"
#include "LTKErrors.h"
#include "LTKException.h"
#include "LTKErrorsList.h"
#include "LTKInkFileReader.h"
#include "LTKTraceGroup.h"
#include "LTKStringUtil.h"
#include "LTKConfigFileReader.h"
#include "LTKShapeRecoUtil.h"
#include "LTKShapeFeatureExtractor.h"
#include "LTKShapeFeature.h"
#include "LTKPreprocessorInterface.h"


//FN_PTR_DELETE_SHAPE_FTR_PTR LTKShapeRecoUtil::m_deleteShapeFeatureFunc = NULL;

/**********************************************************************************
* AUTHOR		: Saravanan. R
* DATE			: 11-01-2007
* NAME			: LTKShapeRecoUtil
* DESCRIPTION	: Constructor
* ARGUMENTS		: None
* RETURNS		: None
* NOTES			: 
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
LTKShapeRecoUtil::LTKShapeRecoUtil()
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<  
        "Entered LTKShapeRecoUtil::LTKShapeRecoUtil()"  <<endl;
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<  
        "Exiting LTKShapeRecoUtil::LTKShapeRecoUtil()"  <<endl;
}

/**********************************************************************************
 * AUTHOR		: Saravanan. R
 * DATE			: 31-01-2007
 * NAME			: LTKShapeRecoUtil
 * DESCRIPTION	: Destructor
 * ARGUMENTS		: None
 * RETURNS		: None
 * NOTES			: 
 * CHANGE HISTROY
 * Author			Date				Description
 *************************************************************************************/
LTKShapeRecoUtil::~LTKShapeRecoUtil()
{
}

/**********************************************************************************
 * AUTHOR		: Saravanan. R
 * DATE			: 11-01-2007
 * NAME			: getAbsolutePath
 * DESCRIPTION	: This method is used to convert the relative path to the absolute path		
 * ARGUMENTS		: pathName     : string : Holds the path of the training file
 *				  lipiRootPath : string : Holds the lipiroot path
 *
 * RETURNS		: SUCCESS only
 * NOTES			: 
 * CHANGE HISTROY
 * Author			Date				Descriptionh
 *************************************************************************************/
int LTKShapeRecoUtil::getAbsolutePath (const string& inputPath, 
        const string& lipiRootPath, 
        string& outPath)
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<  
        "Entered LTKShapeRecoUtil::getAbsolutePath()"  <<endl;

    outPath = "";
    vector<string> tokens;

    int returnStatus = SUCCESS;

    //Split the path name into number of tokens based on the delimter
    returnStatus = LTKStringUtil::tokenizeString(inputPath,  "\\/",  tokens);

    if(returnStatus != SUCCESS)
    {
        LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Error: " <<
            getErrorMessage(returnStatus) << 
            " LTKShapeRecoUtil::getAbsolutePath()"  <<endl;

        LTKReturnError(returnStatus);
    }

    //The first token must be the $LIPI_ROOT. Otherwise return from the function
    if (tokens[0] != LIPIROOT)
    {
		outPath = inputPath;
        return SUCCESS;
    }

    //Store the Environment variable into the tokens
    tokens[0] = lipiRootPath;

    //Reinitialize the outPath
    for(int i=0 ; i < tokens.size() ; i++)
    {
        outPath += tokens[i] + SEPARATOR;  
    }

    // Erase the last character '\'
    outPath.erase(outPath.size()-1,1);

    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<  
        "Exiting LTKShapeRecoUtil::getAbsolutePath()"  <<endl;

    return SUCCESS;
}


/***********************************************************************************
 * AUTHOR		: Saravanan. R
 * DATE			: 19-01-2007
 * NAME			: isProjectDynamic
 * DESCRIPTION	: This method reads the project.cfg to find whether the project 
 *				  is dynamic or not
 * ARGUMENTS		: configFilePath : string : Holds the path of the project.cfg
 *				  numShapes : unsigned short : Holds the NumShapes value from config file
 *				  returnStauts : int : Holds SUCCESS or ErrorValues
 *				  strNumShapes : string : Holds the NumShapes value from config file
 * RETURNS		: true : if the project was dynamic
 *				  false : if the project was dynamic
 * NOTES			: 
 * CHANGE HISTROY
 * Author			Date				Description
 *************************************************************************************/
int LTKShapeRecoUtil::isProjectDynamic(const string& configFilePath, 
        unsigned short& numShapes, 
        string& strNumShapes,
        bool& outIsDynamic )
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<  
        "Entering LTKShapeRecoUtil::isProjectDynamic()"  <<endl;

    //Specifies the project is dynamic or not
    outIsDynamic = false;

    string numShapesCfgAttr = "";

    //As numshapes was unsigned short we use this tempNumShapes as integer,
    //it is used for checking whether it is less than 0
    int tempNumShapes = 0;
    LTKConfigFileReader* projectCfgAttrs = NULL;
    string valueFromCFG = "0";

    int errorCode = SUCCESS;

    try
    {
        //Read the config entries
        projectCfgAttrs = new LTKConfigFileReader(configFilePath);
        errorCode = projectCfgAttrs->getConfigValue(PROJECT_CFG_ATTR_NUMSHAPES_STR, numShapesCfgAttr);

        //Checking whether the numshapes was dynamic
        if(errorCode != SUCCESS)
        {
            LOG( LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: " <<
                "NumShapes should be set to  dynamic or the number of training classes"  <<
                " LTKShapeRecoUtil::isProjectDynamic()"  <<endl;

            LTKReturnError(errorCode);

        }
        else if( LTKSTRCMP(numShapesCfgAttr.c_str(), DYNAMIC) == 0 )
        {
            //Numshapes was dynamic
            outIsDynamic = true;
            tempNumShapes = 0;
        }
        else
        {
            bool isPositiveInteger=true;

            valueFromCFG = numShapesCfgAttr;

            for(int charIndex=0 ; charIndex < valueFromCFG.size() ; ++charIndex)
            {
                if(!(valueFromCFG[charIndex]>='0' && valueFromCFG[charIndex]<='9'))
                {
                    isPositiveInteger=false;
                    break;
                }
            }



            if(!isPositiveInteger)
            {
                LOG( LTKLogger::LTK_LOGLEVEL_ERR)<<"Error" <<
                    "NumShapes should be set to  dynamic or the number of training classes"  <<
                    " LTKShapeRecoUtil::isProjectDynamic()"  <<endl;

                LTKReturnError(EINVALID_NUM_OF_SHAPES);
            }
            else
            {
                tempNumShapes = atoi(valueFromCFG.c_str());
                if(tempNumShapes==0)
                {
                    LOG( LTKLogger::LTK_LOGLEVEL_ERR)<<"Error" <<
                        "NumShapes should be set to  dynamic or the number of training classes"  <<
                        " LTKShapeRecoUtil::isProjectDynamic()"  <<endl;

                    LTKReturnError(EINVALID_NUM_OF_SHAPES);
                }
                else
                {
                    //Numshapes was not dynamic
                    outIsDynamic = false;
                }

            }

        }

        numShapes = tempNumShapes;
        strNumShapes = valueFromCFG;
        LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<  
            "NumShapes in the project is " <<  valueFromCFG <<endl;


    }
    catch(LTKException e)
    {	
        delete projectCfgAttrs;	
        throw e;
    }	

    delete projectCfgAttrs;
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<  
        "Exiting LTKShapeRecoUtil::isProjectDynamic()"  <<endl;

    return SUCCESS;
}



/**********************************************************************************
 * AUTHOR		: Saravanan. R
 * DATE			: 30-01-2007
 * NAME          : readInkFromFile
 * DESCRIPTION   : This method reads the Ink file and check from empty traces
 * ARGUMENTS     : string : Holds the Path of the unipen ink file
string : Holds the Path of the lipi root				  
 * RETURNS       : 
 * NOTES         :
 * CHANGE HISTROY
 * Author            Date                Description
 *************************************************************************************/
int LTKShapeRecoUtil::readInkFromFile(const string& path, const string& lipiRootPath,
        LTKTraceGroup& inTraceGroup, 
        LTKCaptureDevice& captureDevice,
        LTKScreenContext& screenContext)
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<  
        "Entering LTKShapeRecoUtil::readInkFromFile()"  <<endl;

    //inTraceGroup.emptyAllTraces();

    string tempPath = path;

    //Check and convert Relative path to Absolute path
    string outPath = "";
    getAbsolutePath(tempPath, lipiRootPath, outPath);

    //Print the path name
    cout << outPath << endl;	

    //Read Ink file to inTraceGroup
    //    errorVal = LTKInkFileReader::readUnipenInkFile(tempPath,inTraceGroup,captureDevice,screenContext);
    int errorCode = LTKInkFileReader::readUnipenInkFile(outPath,inTraceGroup,captureDevice,screenContext);

    if (errorCode != SUCCESS)
    {
        LOG( LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: " <<
            getErrorMessage(errorCode) << 
            " LTKShapeRecoUtil::readInkFromFile()"  <<endl;

        LTKReturnError(errorCode);
    }

    //Check for empty traces in inTraceGroup
    
    if (inTraceGroup.containsAnyEmptyTrace())
    {
        LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Error: " <<
            "TraceGroup has empty traces" << 
            " LTKShapeRecoUtil::readInkFromFile()"  <<endl;

        LTKReturnError(EEMPTY_TRACE);
    }


    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<  
        "Exiting LTKShapeRecoUtil::readInkFromFile()"  <<endl;

    return SUCCESS;
}

/**********************************************************************************
 * AUTHOR		: Saravanan. R
 * DATE			: 30-01-2007
 * NAME          : checkEmptyTraces
 * DESCRIPTION   : This method checks for empty traces
 * ARGUMENTS     : inTraceGroup : LTKTraceGroup :
 * RETURNS       : 1 if it contains empty trace group, 0 otherwise
 * NOTES         :
 * CHANGE HISTROY
 * Author            Date                Description
 *************************************************************************************/
/*
int LTKShapeRecoUtil::checkEmptyTraces(const LTKTraceGroup& inTraceGroup)
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<  
        "Entering LTKShapeRecoUtil::checkEmptyTraces()"  <<endl;

    int returnVal = SUCCESS;

    vector<LTKTrace> tracesVec = inTraceGroup.getAllTraces();  //traces in trace group

    int numTraces = tracesVec.size();

    int numTracePoints = 0;

    LTKTrace trace;							//	a trace of the trace group

    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<  "numTraces = " << numTraces  <<endl;


    for(int traceIndex=0; traceIndex < numTraces; ++traceIndex) 
    {
        trace = tracesVec.at(traceIndex);

        numTracePoints=trace.getNumberOfPoints();

        if(numTracePoints==0)
        {
            LOG( LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<<
                getError(EEMPTY_TRACE) <<
                "Exiting LTKShapeRecoUtil::checkEmptyTraces()"  <<endl;
            LTKReturnError(EEMPTY_TRACE);
        }

    }

    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<  
        "Exiting LTKShapeRecoUtil::checkEmptyTraces()"  <<endl;
    return SUCCESS;
}
*/
int LTKShapeRecoUtil::convertHeaderToStringStringMap(const string& header, stringStringMap& headerSequence)
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<  
        "Entering LTKShapeRecoUtil::convertHeaderToStringStringMap()"  <<endl;

    vector<string> tokens;
    vector<string> strList;

    int returnStatus = SUCCESS;

    LTKStringUtil::tokenizeString(header, "<>", tokens);

    for(int i=0 ; i < tokens.size(); ++i)
    {
        returnStatus = LTKStringUtil::tokenizeString(tokens[i],  "=",  strList);
        if(returnStatus != SUCCESS)
        {
            LOG( LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: " <<
                getErrorMessage(returnStatus) << 
                " LTKShapeRecoUtil::convertHeaderToStringStringMap()"  <<endl;
            LTKReturnError(returnStatus);
        }
        if(strList.size() == 2)
        {
            headerSequence[strList[0]] = strList[1];
        }
    }
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<  
        "Exiting LTKShapeRecoUtil::convertHeaderToStringStringMap()"  <<endl;

    return SUCCESS;
}


/**********************************************************************************
* AUTHOR		: Saravanan. R
* DATE			: 14-03-2007
* NAME          : shapeFeatureVectorToFloatVector
* DESCRIPTION   : This method converts the ShapeFeatureVector to float vector
* ARGUMENTS     : 
* RETURNS       : 
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description
*************************************************************************************/
int LTKShapeRecoUtil::shapeFeatureVectorToFloatVector(const vector<LTKShapeFeaturePtr>& shapeFeature, 
                                                   floatVector& outFloatVector)
{
    int returnVal = SUCCESS;
    
	//Iterators for the LTKShapeFeature
	vector<LTKShapeFeaturePtr>::const_iterator shapeFeatureIter = shapeFeature.begin();
	vector<LTKShapeFeaturePtr>::const_iterator shapeFeatureIterEnd = shapeFeature.end();


	vector<float> shapeFeatureFloatvector;

	for(; shapeFeatureIter != shapeFeatureIterEnd; ++shapeFeatureIter)
	{
	
		//Convert the shapefeature to float vector
		returnVal = (*shapeFeatureIter)->toFloatVector(shapeFeatureFloatvector);

        if ( returnVal != SUCCESS )
        {
            break;
        }
        
		outFloatVector.insert(outFloatVector.end(),
			                  shapeFeatureFloatvector.begin(),
							  shapeFeatureFloatvector.end());
		
		shapeFeatureFloatvector.clear();
	
	}
	
	return returnVal;
}