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
* OR THE US E OR OTHER DEALINGS IN THE SOFTWARE.
*****************************************************************************************/

/************************************************************************
 * SVN MACROS
 *
 * $LastChangedDate: 2011-02-08 16:57:52 +0530 (Tue, 08 Feb 2011) $
 * $Revision: 834 $
 * $Author: mnab $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Implementation of BoxedFieldRecognizer
 * CONTENTS:
 *
 * AUTHOR:     Deepu V.
 *
 * DATE:       March 23, 2005
 * CHANGE HISTORY:
 * Author		Date			Description of 
 ************************************************************************/

#include "BoxFieldRecognizer.h"
#include "LTKLoggerUtil.h"
#include "LTKTrace.h"
#include "LTKInc.h"
#include "LTKTypes.h"
#include "LTKErrors.h"
#include "LTKErrorsList.h"
#include "LTKTraceGroup.h"
#include "LTKWordRecoConfig.h"

#include "LTKShapeRecognizer.h"
#include "LTKRecognitionContext.h"

#include "LTKScreenContext.h"
#include "LTKCaptureDevice.h"
#include "LTKConfigFileReader.h"
#include "LTKMacros.h"
#include "LTKStrEncoding.h"
#include "LTKException.h"
#include "LTKOSUtilFactory.h"
#include "LTKOSUtil.h"
#include "LTKStringUtil.h"

#include <functional>

extern void *m_hAlgoDLLHandle;



/*****************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 22-AUG-2005
* NAME			: initializeWordRecognizer
* DESCRIPTION	: Initialization of Boxed word Recognizer. This function performs
*                      -Create & initialize shape recognizer
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of
*****************************************************************************/
BoxedFieldRecognizer::BoxedFieldRecognizer(const LTKControlInfo& controlInfo)
:LTKWordRecognizer(BOXFLD),
m_shapeRecognizer(NULL),
m_numShapeRecoResults(DEFAULT_SHAPE_RECO_CHOICES),
m_shapeRecoMinConfidence(DEFAULT_SHAPE_RECO_MIN_CONFID),
m_module_createShapeRecognizer(NULL),
m_module_deleteShapeRecognizer(NULL),
m_numCharsProcessed(0),
m_numTracesProcessed(0),
m_boxedShapeProject(""),
m_boxedShapeProfile(""),
m_lipiRoot(""),
m_lipiLib(""),
m_boxedConfigFile(""),
m_logFile(""),
m_logLevel(LTKLogger::LTK_LOGLEVEL_ERR),
m_toolkitVersion(""),
m_OSUtilPtr(LTKOSUtilFactory::getInstance())
{

	string boxedShapeProfile;   //profile name

	int errorCode = 0;

	LTKControlInfo tempControlInfo = controlInfo;

    if ( tempControlInfo.lipiRoot.empty() )
    {
        throw LTKException(ELIPI_ROOT_PATH_NOT_SET);
    }

    if ( tempControlInfo.projectName.empty() )
    {
        throw LTKException(EINVALID_PROJECT_NAME);
    }

    if( (tempControlInfo.profileName).empty() )
	{
		tempControlInfo.profileName = DEFAULT_PROFILE;
	}

    if ( tempControlInfo.toolkitVersion.empty() )
    {
        throw LTKException(ENO_TOOLKIT_VERSION);
    }

    // initialize the data members
    m_lipiRoot          = tempControlInfo.lipiRoot;
    m_lipiLib           = tempControlInfo.lipiLib;
    m_toolkitVersion    = tempControlInfo.toolkitVersion;


	//constructing the boxed Config filename
	m_boxedConfigFile  = m_lipiRoot + PROJECTS_PATH_STRING +
		                 tempControlInfo.projectName + PROFILE_PATH_STRING +
		                 tempControlInfo.profileName + SEPARATOR + BOXFLD + CONFIGFILEEXT;

	readClassifierConfig();


	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
	    <<"Entering: BoxedFieldRecognizer::BoxedFieldRecognizer(const LTKControlInfo& )"
	    <<endl;

	//Creating the shape recognizer object
	if((errorCode = createShapeRecognizer(m_boxedShapeProject, m_boxedShapeProfile,&m_shapeRecognizer)) != SUCCESS)
	{
		 LOG(LTKLogger::LTK_LOGLEVEL_ERR)
        <<"Error: BoxedFieldRecognizer::BoxedFieldRecognizer(const LTKControlInfo&)"<<endl;

	   throw LTKException(errorCode);
	}

	if(m_shapeRecognizer == NULL)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
		    <<"Error : "<< ECREATE_SHAPEREC <<":"<< getErrorMessage(ECREATE_SHAPEREC)
            <<" BoxedFieldRecognizer::BoxedFieldRecognizer(const LTKControlInfo&)" <<endl;

		throw LTKException(ECREATE_SHAPEREC);
	}

	//loading the model data file
	if( (errorCode = (m_shapeRecognizer->loadModelData())) != SUCCESS )
	{
		m_module_deleteShapeRecognizer(m_shapeRecognizer);
		m_shapeRecognizer = NULL;

		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
        	<<"Error: BoxedFieldRecognizer::BoxedFieldRecognizer(const LTKControlInfo&)"<<endl;

		throw LTKException(errorCode);
	}

	m_numCharsProcessed = 0;  //initializing number of characters processed
	m_numTracesProcessed = 0; //initializing number of traces processed


	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
	    <<"Exiting: BoxedFieldRecognizer::BoxedFieldRecognizer(const LTKControlInfo&)"
	    <<endl;
}

/******************************************************************************
 * AUTHOR		: Nidhi Sharma
 * DATE			: 28-Sept-2007
 * NAME			: readClassifierConfig
 * DESCRIPTION	: Reads the boxfld.cfg and initializes the data members of the class
 * ARGUMENTS		: none
 * RETURNS		: SUCCESS   - If config file read successfully
 *				  errorCode - If failure
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int BoxedFieldRecognizer::readClassifierConfig()
{

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
	    <<"Entering: BoxedFieldRecognizer::readClassifierConfig"
	    <<endl;

    LTKConfigFileReader* boxedFldConfigMap = NULL;
    string cfgFileValue = "";
	int errorCode = FAILURE;

    try
	{
		boxedFldConfigMap = new LTKConfigFileReader(m_boxedConfigFile);
	}
	catch(LTKException fofe)
	{
		delete boxedFldConfigMap;

		 LOG(LTKLogger::LTK_LOGLEVEL_ERR)
        <<"Error: BoxedFieldRecognizer::readClassifierConfig"<<endl;

		LTKReturnError(ECONFIG_FILE_OPEN);	// Error while reading project.cfg
	}

	//initializing the number of shape recognition choices required from the file

    errorCode = boxedFldConfigMap->getConfigValue(NUMSHAPECHOICES, cfgFileValue);

    if ( errorCode == SUCCESS )
    {
	    m_numShapeRecoResults = atoi(cfgFileValue.c_str());

		if(m_numShapeRecoResults <= 0)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			    <<"Error : "<< ENON_POSITIVE_NUM <<":"<< getErrorMessage(ENON_POSITIVE_NUM)
                <<" BoxedFieldRecognizer::readClassifierConfig" <<endl;

			LTKReturnError(ENON_POSITIVE_NUM);
		}

    }
	else
	{
		LOG(LTKLogger::LTK_LOGLEVEL_INFO)
			<<"Assuming default value for number of shape recognizer choices:"
			<<m_numShapeRecoResults<<endl;
	}



	//initializing the minimum confidence threshold

    cfgFileValue = "";
    errorCode = boxedFldConfigMap->getConfigValue(MINSHAPECONFID, cfgFileValue);

    if ( errorCode == SUCCESS )
    {
        m_shapeRecoMinConfidence = LTKStringUtil::convertStringToFloat(cfgFileValue);

		if(m_shapeRecoMinConfidence < 0 || m_shapeRecoMinConfidence > 1)
		{

			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			    <<"Error : "<< EINVALID_CONFIDENCE_VALUE <<":"<< getErrorMessage(EINVALID_CONFIDENCE_VALUE)
                <<" BoxedFieldRecognizer::readClassifierConfig" <<endl;


			LTKReturnError(EINVALID_CONFIDENCE_VALUE);
		}
    }
	else
	{
			LOG(LTKLogger::LTK_LOGLEVEL_INFO)
			<<"Assuming default value for minimum shape recognizer confidence:"
			<<m_shapeRecoMinConfidence<<endl;

	}

	//retrieving the boxed shape project and profile
	cfgFileValue = "";
    errorCode = boxedFldConfigMap->getConfigValue(BOXEDSHAPEPROJECT, cfgFileValue);

    if ( errorCode == SUCCESS )
    {
    	m_boxedShapeProject = cfgFileValue;

		if(m_boxedShapeProject.empty())
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			    <<"Error : "<< EINVALID_PROJECT_NAME <<":"<< getErrorMessage(EINVALID_PROJECT_NAME)
			    <<" BoxedFieldRecognizer::readClassifierConfig" <<endl;

			LTKReturnError(EINVALID_PROJECT_NAME);
		}
    }
	else
	{
         LOG(LTKLogger::LTK_LOGLEVEL_ERR)
             <<"Error : "<< ENO_SHAPE_RECO_PROJECT <<":"<< getErrorMessage(ENO_SHAPE_RECO_PROJECT)
             <<" BoxedFieldRecognizer::readClassifierConfig" <<endl;

		LTKReturnError(ENO_SHAPE_RECO_PROJECT);
	}


    //retrieving the boxed shape project and profile
	cfgFileValue = "";
    errorCode = boxedFldConfigMap->getConfigValue(BOXEDSHAPEPROFILE, cfgFileValue);

    if( errorCode == SUCCESS )
    {
    	m_boxedShapeProfile = cfgFileValue;

		if(m_boxedShapeProfile.empty())
		{
			LOG(LTKLogger::LTK_LOGLEVEL_INFO)
				<<"No profile specified for shape recognizer project.Assuming 'default' profile"<<endl;

			m_boxedShapeProfile = DEFAULT_PROFILE;
		}


    }
	else
	{
		LOG(LTKLogger::LTK_LOGLEVEL_INFO)
			<<"No profile specified for shape recognizer project. Assuming 'default' profile"<<endl;

		m_boxedShapeProfile = DEFAULT_PROFILE;

	}

    delete boxedFldConfigMap;

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
		<<"Exiting: BoxedFieldRecognizer::readClassifierConfig"
	    <<endl;

	return SUCCESS;
}

/*****************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 22-AUG-2005
* NAME			: processInk
* DESCRIPTION	: This method is called from recognition context whenever new traces
*               : are added to it.  The Recognizer need to process the new traces
*               : in this methods and updates the internal state.
* ARGUMENTS		: rc - The recognition context for the current recognition
* RETURNS		: SUCCESS/FAILURE
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of
******************************************************************************/
int BoxedFieldRecognizer::processInk (LTKRecognitionContext& rc)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
	    <<"Entering: BoxedFieldRecognizer::processInk"
	    <<endl;

	string tempStr = REC_UNIT_INFO;

	int tempFlagValue=0;

	int errorCode=0;

	if((errorCode=rc.getFlag(tempStr,tempFlagValue))!=SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
            <<"Error: BoxedFieldRecognizer::processInk"<<endl;

		LTKReturnError(errorCode);
	}

	//give an error if the Ink is not segmented into characters
	if(tempFlagValue != REC_UNIT_CHAR)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
		    <<"Error : "<< EINVALID_SEGMENT <<":"<< getErrorMessage(EINVALID_SEGMENT)
            <<" BoxedFieldRecognizer::processInk" <<endl;

		LTKReturnError(EINVALID_SEGMENT);
	}

	tempStr = REC_MODE;


	if((errorCode=rc.getFlag(tempStr,tempFlagValue))!=SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
            <<"Error: BoxedFieldRecognizer::processInk"<<endl;

		LTKReturnError(errorCode);
	}

	//if the recognizer mode is correct
	if (tempFlagValue == REC_MODE_STREAMING)
	{
		//recognize the newly added strokes
		recognizeTraces(rc);
	}
	else
	{
		//give an error otherwise
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			<<"Error : "<< EINVALID_REC_MODE <<":"<< getErrorMessage(EINVALID_REC_MODE)
            <<" BoxedFieldRecognizer::processInk" <<endl;


		LTKReturnError(EINVALID_REC_MODE);
	}

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
		<<"Exiting: BoxedFieldRecognizer::processInk"
	    <<endl;

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 22-AUG-2005
* NAME			: endRecoUnit
* DESCRIPTION	: This function notifies the recognizer that end of current ink is
*               : the end of a logic segment.  This information could be used in
*               : constraining the recognizer choices
* ARGUMENTS		:
* RETURNS		: SUCCESS/FAILURE
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of 
*************************************************************************************/
void BoxedFieldRecognizer::endRecoUnit ()
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
	    <<"Entering: BoxedFieldRecognizer::endRecoUnit"<<endl;

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
		<<"Exiting: BoxedFieldRecognizer::endRecoUnit"
	    <<endl;

}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 22-AUG-2005
* NAME			: recognize
* DESCRIPTION	: This is the recognize call
*               : The results of the recognition is set on the Recognition context
*               : object.  In case of BATCH_MODE recognition recognition of every
*               : character is performed.  otherwise the recognizer updates the outputs
*               : with the recognized results
* ARGUMENTS		: rc - The recognition context for the current recognition
* RETURNS		: SUCCESS/FAILURE
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of
*************************************************************************************/
int BoxedFieldRecognizer::recognize (LTKRecognitionContext& rc)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
	    <<"Entering: BoxedFieldRecognizer::recognize"
	    <<endl;

	string 	tempStr = REC_UNIT_INFO;  //temp string required to pass the arguments to set/get Flags

	int tempFlagValue = 0; //temp int to hold flag values

	int errorCode = 0;

	vector <LTKWordRecoResult>::iterator resultIter,resultEnd;  //iterates through decoded recognition results

	int numWordRecoResults ; //Number of results required by the application

	int resultIndex ; //index to iterate through the results

	vector<unsigned short> resultString; //result

	float normConf; //normalized confidence

	//Returning FAILURE if the recognition context
	//is not segmented into characters

	if((errorCode=rc.getFlag(tempStr,tempFlagValue))!=SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
            <<"Error: BoxedFieldRecognizer::recognize"<<endl;

		LTKReturnError(errorCode);
	}

	if( tempFlagValue != REC_UNIT_CHAR)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
		    <<"Error : "<< EINVALID_SEGMENT <<":"<< getErrorMessage(EINVALID_SEGMENT)
            <<" BoxedFieldRecognizer::recognize" <<endl;

		LTKReturnError(EINVALID_SEGMENT);
	}

	tempStr =REC_MODE;

	if((errorCode=rc.getFlag(tempStr,tempFlagValue))!=SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
            <<"Error: BoxedFieldRecognizer::recognize"<<endl;

		LTKReturnError(errorCode);
	}

	if(tempFlagValue == REC_MODE_BATCH)
	{
		//clear all the recognizer state
		clearRecognizerState();
		recognizeTraces(rc);
	}
	else if (tempFlagValue == REC_MODE_STREAMING)
	{
		recognizeTraces(rc);
	}
	else
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
		    <<"Error : "<< EINVALID_REC_MODE <<":"<< getErrorMessage(EINVALID_REC_MODE)
            <<" BoxedFieldRecognizer::recognize" <<endl;

		LTKReturnError(EINVALID_REC_MODE);
	}

	/* Now all the recognized results are in
	 * m_decodedResults.
	 */

	resultEnd =	m_decodedResults.end();

	for(resultIter = m_decodedResults.begin(); resultIter != resultEnd ; ++resultIter)
	{
		normConf = (*resultIter).getResultConfidence();

		normConf /= ((*resultIter).getResultWord()).size();

		(*resultIter).setResultConfidence(normConf);

	}

	//number of requested results
	numWordRecoResults =  rc.getNumResults();

	//checking with existing recognition results' size
	if(numWordRecoResults > m_decodedResults.size())
	{
		LOG(LTKLogger::LTK_LOGLEVEL_INFO)
			<< "Don't have enough results to populate. Num of results available = "
			<< m_decodedResults.size() <<", however, results asked for ="
			<< numWordRecoResults <<endl;
	}

	resultEnd = m_decodedResults.end();
	for(resultIndex =0,resultIter = m_decodedResults.begin();(resultIndex <numWordRecoResults)&&(resultIter != resultEnd); ++resultIndex,++resultIter)
	{
		//map the shape ids to unicode IDs
		if((errorCode = LTKStrEncoding::shapeStrToUnicode(m_boxedShapeProject,(*resultIter).getResultWord(),resultString)) != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
            	<<"Error: BoxedFieldRecognizer::recognize"<<endl;

			LTKReturnError(errorCode);
		}

		//adding the recognition result to recognition context
		 rc.addRecognitionResult(LTKWordRecoResult(resultString,
												   (*resultIter).getResultConfidence()));

		resultString.clear();

	}

	//clearing state of the recognizer
	clearRecognizerState();


	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
		<<"Exiting: BoxedFieldRecognizer::recognize"
	    <<endl;

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 22-AUG-2005
* NAME			: recognize
* DESCRIPTION	: This method reset the recognizer
* ARGUMENTS		: resetParam - This parameter could specify what to reset
* RETURNS		: SUCCESS/FAILURE
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of 
*************************************************************************************/
 int BoxedFieldRecognizer::reset (int resetParam)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
	    <<"Entering: BoxedFieldRecognizer::reset"<<endl;

	clearRecognizerState();

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
		<<"Exiting: BoxedFieldRecognizer::reset"
	    <<endl;

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 22-AUG-2005
* NAME			: recognize
* DESCRIPTION	: This method reset the recognizer
* ARGUMENTS		: resetParam - This parameter could specify what to reset
* RETURNS		: SUCCESS/FAILURE
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of
*************************************************************************************/
 int BoxedFieldRecognizer::unloadModelData()
{

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
	    <<"Entering: BoxedFieldRecognizer::unloadModelData"
	    <<endl;

	 //clear the recognition state
	clearRecognizerState();

	int errorCode=FAILURE;

	//unload the model data and
	//delete the shape recognizer
	if( m_shapeRecognizer  && (m_module_deleteShapeRecognizer != NULL) )
	{

	   if((errorCode = m_shapeRecognizer->unloadModelData()) != SUCCESS)
	   {
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
            	<<"Error: BoxedFieldRecognizer::unloadModelData"<<endl;


		   LTKReturnError(errorCode);
	   }

	   if((errorCode = m_module_deleteShapeRecognizer(m_shapeRecognizer)) != SUCCESS)
	   {
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
            	<<"Error: BoxedFieldRecognizer::unloadModelData"<<endl;


		   LTKReturnError(errorCode);
	   }

	   m_shapeRecognizer = NULL;
	}

	//Freeing the shape recognition library
	if(m_hAlgoDLLHandle)
	{
		m_OSUtilPtr->unloadSharedLib(m_hAlgoDLLHandle);
		m_hAlgoDLLHandle = NULL;
	}

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
		<<"Exiting: BoxedFieldRecognizer::unloadModelData"
		<<endl;

	return SUCCESS;
}
/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 22-AUG-2005
* NAME			: ~BoxedFieldRecognizer
* DESCRIPTION	: destructor
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of
*************************************************************************************/

BoxedFieldRecognizer::~BoxedFieldRecognizer()
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
	    <<"Entering: BoxedFieldRecognizer::~BoxedFieldRecognizer"
	    <<endl;

	//unload the model data

	int errorCode = FAILURE;
	if((errorCode = unloadModelData()) != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
        	<<"Error: BoxedFieldRecognizer::~BoxedFieldRecognizer"<<endl;

		throw LTKException(errorCode);
	}

    delete m_OSUtilPtr;

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
		<<"Exiting: BoxedFieldRecognizer::~BoxedFieldRecognizer"
		<<endl;


}


//PRIVATE FUNCTIONS

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 22-AUG-2005
* NAME			: clearRecognizerState
* DESCRIPTION	: Erase the state information of the recognizer
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of
*************************************************************************************/

void BoxedFieldRecognizer::clearRecognizerState()
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
	    <<"Entering: BoxedFieldRecognizer::clearRecognizerState"
	    <<endl;

	m_numCharsProcessed = 0;       //initializing number of characters processed
	m_numTracesProcessed = 0;      //initializing number of traces processed
	m_decodedResults.clear();      //clearing all the partially decoded results
	m_boxedChar = LTKTraceGroup(); //assigning a new empty LTKTraceGroup

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
		<<"Exiting: BoxedFieldRecognizer::clearRecognizerState"
		<<endl;

}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 22-AUG-2005
* NAME			: recognizeTraces
* DESCRIPTION	: performs the recognition of the new strokes added to rc
*               : pre condition - markers are present in this vector
*               :               - m_numTracesProcessed  and m_numCharsProcessed
*               :                 set to proper value
* ARGUMENTS		: rc - The recognitino context
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of
*************************************************************************************/
int BoxedFieldRecognizer::recognizeTraces(LTKRecognitionContext& rc )
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
	    <<"Entering: BoxedFieldRecognizer::recognizeTraces"
	    <<endl;

	vector<LTKTrace>::const_iterator traceIter,traceEnd,traceBegin;
	                 //iterator for the traces

	int errorCode = FAILURE;

	int recUnit;     //unit for recognition (should be char)

	LTKTraceGroup emptyChar;
	                 //TraceGroup object that buffers
	                 //all ink corresponding to a character

	vector<int> subSet;
	                 //passing a null arguement for shape subset

	vector<LTKShapeRecoResult> shapeRecoResults;
	                 //The object to hold the output from shape recognizer

	LTKScreenContext screenContext = rc.getScreenContext();
	                 //retrieving the screen context

	LTKCaptureDevice captureDevice = rc.getDeviceContext();
	                 //retrieving the device context

	const LTKTraceVector & traces = rc.getAllInk();
	                //retrieving the traces from recognition context

	string tempStr;  //temporary string object


	if(m_shapeRecognizer == NULL)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) <<"Shape recognizer not initialized" <<endl;

		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
		    <<"Error : "<< ENULL_POINTER <<":"<< getErrorMessage(ENULL_POINTER)
            <<" BoxedFieldRecognizer::recognizeTraces" <<endl;

		LTKReturnError(ENULL_POINTER);

	}
	else if( (errorCode = m_shapeRecognizer->setDeviceContext(captureDevice)) != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Unable to set device context in shape rec : " << getErrorMessage(errorCode) <<endl;

		 LOG(LTKLogger::LTK_LOGLEVEL_ERR)
             <<"Error: BoxedFieldRecognizer::recognizeTraces"<<endl;

		LTKReturnError(errorCode);
	}

	shapeRecoResults.reserve(m_numShapeRecoResults+1);//reserving memory


	if(m_numTracesProcessed > traces.size())
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Invalid number of traces processed. "
		    << "Traces processed = " << m_numTracesProcessed
            <<  " > total number of traces" << traces.size() <<endl;

        LOG(LTKLogger::LTK_LOGLEVEL_ERR)
		             <<"Error : "<< EINVALID_NUM_OF_TRACES <<":"<< getErrorMessage(EINVALID_NUM_OF_TRACES)
             <<" BoxedFieldRecognizer::recognizeTraces" <<endl;

		LTKReturnError(EINVALID_NUM_OF_TRACES);
	}
	//Start processing from the number of traces processed.
	traceBegin = traces.begin() + m_numTracesProcessed;
	traceEnd = traces.end();int r=0;

	for(traceIter = traceBegin; traceIter != traceEnd; ++traceIter)
	{
		/* Marker strokes are inserted to detect
		 * end of segment.  The marker strokes are
		 * identified by 9number of channels == 0)
         */
		if((*traceIter).getNumberOfPoints() == 0)
		{
			tempStr = REC_UNIT_INFO;
			if((errorCode = rc.getFlag(tempStr,recUnit)) != SUCCESS)
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)
            		<<"Error: BoxedFieldRecognizer::recognizeTraces"<<endl;

				LTKReturnError(errorCode);
			}
			switch(recUnit)
			{

			/* The segment is character
			 * This algorithm recognizes
			 * only character segments
			 */
			case REC_UNIT_CHAR:
				shapeRecoResults.clear();
				//calling the shape recognizer's recognize method.

				if(m_boxedChar.getNumTraces() == 0)
				{
					LTKShapeRecoResult T;
					T.setShapeId(SHRT_MAX);
					T.setConfidence(1.0);
					shapeRecoResults.push_back(T);
				}
				else if( (errorCode =	m_shapeRecognizer->recognize(m_boxedChar,screenContext,subSet,
					m_shapeRecoMinConfidence, m_numShapeRecoResults, shapeRecoResults ))!= SUCCESS )
				{
					LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Shape recognition failed : " << getErrorMessage(errorCode) <<endl;

					LOG(LTKLogger::LTK_LOGLEVEL_ERR)
            			<<"Error: BoxedFieldRecognizer::recognizeTraces"<<endl;

					LTKReturnError(errorCode);
				}


				//This updates the recognition results using
				//current shape recognition results

				if((errorCode = updateRecognitionResults(shapeRecoResults,rc)) != SUCCESS)
				{
					LOG(LTKLogger::LTK_LOGLEVEL_ERR)
            			<<"Error: BoxedFieldRecognizer::recognizeTraces"<<endl;

					LTKReturnError(errorCode);
				}

				for(r=0;r<shapeRecoResults.size();++r)
				{
					LTKShapeRecoResult& tempResult=shapeRecoResults[r];

				}

				m_boxedChar = emptyChar;//making the trace group empty again
				break;

			default:
				LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Unsupported reccognizer mode by Box Field" <<endl;

				LOG(LTKLogger::LTK_LOGLEVEL_ERR)
				    <<"Error : "<< EINVALID_RECOGNITION_MODE <<":"<< getErrorMessage(EINVALID_RECOGNITION_MODE)
             		<<" BoxedFieldRecognizer::recognizeTraces" <<endl;

				LTKReturnError(EINVALID_RECOGNITION_MODE);

			}
			++m_numCharsProcessed;      //incrementing number of characters processed
		}
		else
		{
			m_boxedChar.addTrace(*traceIter); //buffering the trace to the temp TraceGroup for recognition
		}
		++m_numTracesProcessed;         //incrementing the number of traces processed
	}


	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
		<<"Exiting: BoxedFieldRecognizer::recognizeTraces"
		<<endl;

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 22-AUG-2005
* NAME			: updateRecognitionResults
* DESCRIPTION	: This function tries to update the
*               : shape recognition choices with new shape recognition results
* ARGUMENTS		: results - new results for updating the results
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of
*************************************************************************************/
int BoxedFieldRecognizer::updateRecognitionResults(const vector<LTKShapeRecoResult>& results, LTKRecognitionContext& rc)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
	    <<"Entering: BoxedFieldRecognizer::updateRecognitionResults"
	    <<endl;

    multimap< float, pair<int,int>, greater<float> >backTrace;
	                   //A multi map is used for finding best N paths
	multimap< float, pair<int,int>, greater<float> >::iterator iter, iterend;
	                   //Iterator for accessing elements of the map
	pair<int,int> combination;
	                   //Temporary variable that keeps a (int,int) pair
	int wordResultIndex, shapeResultIndex;
	                   //loop index variables
	float wordConfidence, shapeConfidence;
	                   //word level and shape level confidences
	unsigned short newSymbol;
	                   //temporary storage for shape recognizer id
	float newConf;     //temporary storage for shape recognizer confidence

	vector<LTKWordRecoResult> newResultVector;
	                   //new results after finding the best N paths

	int numWordRecoResults = rc.getNumResults();
	                   //number of word recognition results requested
	int numShapeRecoResults = results.size();
	                   //number of choices from the shape recognizer
        vector<unsigned short>initVec;
                           //for initializing the trellis



	//If there is no decoded results (First shape recognition in the word)
    if(m_decodedResults.empty())
	{
		//Initializing the results vector
		m_decodedResults.assign(numShapeRecoResults,LTKWordRecoResult());

		//iterating through different word recognition choices
 		for(wordResultIndex = 0; (wordResultIndex<numShapeRecoResults); ++wordResultIndex)
		{
				//Retrieving the shape recognition choices

				newSymbol = results.at(wordResultIndex).getShapeId();
				newConf   = results.at(wordResultIndex).getConfidence();

				//updating the results

				initVec.assign(1,newSymbol);
				m_decodedResults.at(wordResultIndex).setWordRecoResult(initVec,newConf);

		}
	}

    else
	{
		//initializing a temporary result vector
		//newResultVector.assign(smallerResultNumber,LTKWordRecoResult());

		//iterating through each word recognition result
		for(wordResultIndex=0; wordResultIndex<m_decodedResults.size(); ++wordResultIndex)
		{
			wordConfidence = (m_decodedResults.at(wordResultIndex)).getResultConfidence();

			//iterating through each shape recognition results
			for(shapeResultIndex =0; shapeResultIndex<numShapeRecoResults; ++shapeResultIndex )
			{
				//adding total confidence to the map. so that later they
				//can be retrieved in the sorted order
				shapeConfidence = (results.at(shapeResultIndex)).getConfidence();
				backTrace.insert( pair<float, pair<int,int> >( (shapeConfidence+wordConfidence),
					pair<int,int>(wordResultIndex,shapeResultIndex)));
			}
		}

		iterend = backTrace.end();

		//iterating through the map to retrieve the largest confidences.
		for(wordResultIndex = 0,iter = backTrace.begin(); (wordResultIndex<numWordRecoResults)&&(iter!= iterend); ++wordResultIndex,++iter)
		{

			//confidence
			wordConfidence = (*iter).first;

			//the combination that gave this
			//confidence
			combination = (*iter).second;

			//copying the word reco result corresponding to
			//the combination to new result vector
			//newResultVector.at(wordResultIndex) = m_decodedResults.at(combination.first);
			LTKWordRecoResult tempWordRecoResult = m_decodedResults.at(combination.first);

			//retrieving the shape recognition result id
			//and confidence corresponding to the combination
			newSymbol = results.at(combination.second).getShapeId();
			newConf   = results.at(combination.second).getConfidence();

			//updating the word reco result with new id and confidence
			//newResultVector.at(wordResultIndex).updateWordRecoResult(newSymbol, newConf);

				tempWordRecoResult.updateWordRecoResult(newSymbol,newConf);
				newResultVector.push_back(tempWordRecoResult);
		}

		//assigning the newly created result vector
		m_decodedResults = newResultVector;
	}


	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
		<<"Exiting: BoxedFieldRecognizer::updateRecognitionResults"
		<<endl;


	return SUCCESS;
}


/**********************************************************************************
* AUTHOR        : Thanigai
* DATE          : 29-JUL-2005
* NAME          : createShapeRecognizer
* DESCRIPTION   : create an instance of shape recognizer object and call initialize
*				  function. Also loads the model data.
* ARGUMENTS     : strProjectName - project name; strProfileName - profile name
* RETURNS       : handle to the recognizer on success & NULL on error
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of
*************************************************************************************/
 int BoxedFieldRecognizer::createShapeRecognizer(const string& strProjectName, const string& strProfileName,LTKShapeRecognizer** outShapeRecPtr)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
        <<"Entering: BoxedFieldRecognizer::createShapeRecognizer"
        <<endl;


    LTKConfigFileReader* projectCfgFileEntries = NULL;
    LTKConfigFileReader* profileCfgFileEntries = NULL;

	string cfgFilePath      = "";
	string shapeRecDllPath  = "";
	int iResult             = 0;
    string recognizerName   = "";
    string strLocalProfileName(strProfileName);


	/* invalid or no entry for project name */
	if(strProjectName == "")
	{

		*outShapeRecPtr = NULL;

		LOG( LTKLogger::LTK_LOGLEVEL_ERR)
            <<"Invalid or no entry for project name" <<endl;

        LOG(LTKLogger::LTK_LOGLEVEL_ERR)
		    <<"Error : "<< EINVALID_PROJECT_NAME <<":"<< getErrorMessage(EINVALID_PROJECT_NAME)
            <<" BoxedFieldRecognizer::createShapeRecognizer" <<endl;

		LTKReturnError(EINVALID_PROJECT_NAME);
	}

	if(strProfileName == "")
	{
		strLocalProfileName = DEFAULT_PROFILE; /* assume the "default" profile */
	}

	cfgFilePath = m_lipiRoot + PROJECTS_PATH_STRING + strProjectName +
                  PROFILE_PATH_STRING + PROJECT_CFG_STRING;

	try
	{
		projectCfgFileEntries = new LTKConfigFileReader(cfgFilePath);
	}
	catch(LTKException e)
	{
		delete projectCfgFileEntries;

		*outShapeRecPtr = NULL; // Error exception thrown...

		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
            <<"Error: BoxedFieldRecognizer::createShapeRecognizer"<<endl;

		LTKReturnError(e.getErrorCode());


	}

	//	Read the project.cfg and ensure this is a shaperecognizer; i.e. ProjectType = SHAPEREC;
	string projectType = "";
	projectCfgFileEntries->getConfigValue(PROJECT_TYPE_STRING, projectType);

	/* Invalid configuration entry for ProjectType */
	if(projectType != PROJECT_TYPE_SHAPEREC)
	{
		*outShapeRecPtr = NULL;

		int errorCode = EINVALID_CONFIG_ENTRY;

		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
		    <<"Error : "<< EINVALID_CONFIG_ENTRY <<":"<< getErrorMessage(EINVALID_CONFIG_ENTRY)
            <<" BoxedFieldRecognizer::createShapeRecognizer" <<endl;

		LTKReturnError(errorCode);

	}

	//	Read the profile.cfg and find out the recognition module to load;
	cfgFilePath = m_lipiRoot + PROJECTS_PATH_STRING + strProjectName +
	              PROFILE_PATH_STRING + strLocalProfileName +
	              SEPARATOR + PROFILE_CFG_STRING;
	try
	{
		profileCfgFileEntries = new LTKConfigFileReader(cfgFilePath);
	}
	catch(LTKException e)
	{
		*outShapeRecPtr = NULL;

		delete profileCfgFileEntries;

		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
            <<"Error: BoxedFieldRecognizer::createShapeRecognizer"<<endl;

		LTKReturnError(e.getErrorCode());

	}

	int errorCode = profileCfgFileEntries->getConfigValue(SHAPE_RECOGNIZER_STRING, recognizerName);

	/* No recognizer specified. */
	if(errorCode != SUCCESS)
	{

		*outShapeRecPtr = NULL;

		errorCode = ENO_SHAPE_RECOGNIZER;

		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
		    <<"Error : "<< ENO_SHAPE_RECOGNIZER <<":"<< getErrorMessage(ENO_SHAPE_RECOGNIZER)
            <<" BoxedFieldRecognizer::createShapeRecognizer" <<endl;

        delete projectCfgFileEntries;
    	delete profileCfgFileEntries;

		LTKReturnError(errorCode);
	}

    m_hAlgoDLLHandle = NULL;
    errorCode = m_OSUtilPtr->loadSharedLib(m_lipiLib, recognizerName, &m_hAlgoDLLHandle);

	// Unable to load dll
	if(errorCode != SUCCESS)
	{
		*outShapeRecPtr = NULL;

		errorCode = ELOAD_SHAPEREC_DLL;

		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
		    <<"Error : "<< ELOAD_SHAPEREC_DLL <<":"<< getErrorMessage(ELOAD_SHAPEREC_DLL)
            <<" BoxedFieldRecognizer::createShapeRecognizer" <<endl;

        delete projectCfgFileEntries;
    	delete profileCfgFileEntries;

		LTKReturnError(errorCode);

	}

	// Map Algo DLL functions...

	// Unable to map the functions
	if((errorCode = mapShapeAlgoModuleFunctions()) != SUCCESS)
	{
		*outShapeRecPtr = NULL;
	    delete projectCfgFileEntries;
    	delete profileCfgFileEntries;

		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
            <<"Error: BoxedFieldRecognizer::createShapeRecognizer"<<endl;

		LTKReturnError(errorCode)
	}

    // Construct LTKControlInfo object
    LTKControlInfo controlInfo;
    controlInfo.projectName     = strProjectName;
    controlInfo.profileName     = strLocalProfileName;
    controlInfo.lipiRoot        = m_lipiRoot;
    controlInfo.lipiLib         = m_lipiLib;
    controlInfo.toolkitVersion  = m_toolkitVersion;

	*outShapeRecPtr = NULL;

	/* Error, unable to create shape recognizer instance */
	if((errorCode = m_module_createShapeRecognizer(controlInfo,outShapeRecPtr)) != SUCCESS)
	{

		*outShapeRecPtr = NULL;

		delete projectCfgFileEntries;
    	delete profileCfgFileEntries;

		errorCode = ECREATE_SHAPEREC;

		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
		    <<"Error : "<< ECREATE_SHAPEREC <<":"<< getErrorMessage(ECREATE_SHAPEREC)
            <<" BoxedFieldRecognizer::createShapeRecognizer" <<endl;



		LTKReturnError(errorCode);
	}


	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) <<
        "Exiting BoxedFieldRecognizer::createShapeRecognizer" << endl;

	delete projectCfgFileEntries;
	delete profileCfgFileEntries;

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
		<<"Exiting: BoxedFieldRecognizer::createShapeRecognizer"
		<<endl;

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR        : Thanigai
* DATE          : 29-JUL-2005
* NAME          : mapShapeAlgoModuleFunctions
* DESCRIPTION   : To map function addresses of the methods exposed by
*				  the shape recognition modules
* ARGUMENTS     : None
* RETURNS       : 0 on success and other values on error
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of
*************************************************************************************/
int BoxedFieldRecognizer::mapShapeAlgoModuleFunctions()
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
	    <<"Entering: BoxedFieldRecognizer::mapShapeAlgoModuleFunctions"
	    <<endl;

    int returnVal = SUCCESS;
	m_module_createShapeRecognizer = NULL;

    void* functionHandle = NULL;
    returnVal = m_OSUtilPtr->getFunctionAddress(m_hAlgoDLLHandle, 
                                            CREATESHAPERECOGNIZER_FUNC_NAME, 
                                            &functionHandle);
    
	if(returnVal != SUCCESS)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) <<
            "Exported function not found in module : createShapeRecognizer "<<endl;

        LOG(LTKLogger::LTK_LOGLEVEL_ERR)
		    <<"Error : "<< EDLL_FUNC_ADDRESS <<":"<< getErrorMessage(EDLL_FUNC_ADDRESS)
            <<" BoxedFieldRecognizer::mapShapeAlgoModuleFunctions" <<endl;

		LTKReturnError(EDLL_FUNC_ADDRESS);
		// ERROR: Unable to link with createShapeRecognizer function in module */
	}

    m_module_createShapeRecognizer = (FN_PTR_CREATESHAPERECOGNIZER)functionHandle;

    functionHandle = NULL;
    

    // map delete shape recognizer function
    returnVal = m_OSUtilPtr->getFunctionAddress(m_hAlgoDLLHandle, 
                                            DELETESHAPERECOGNIZER_FUNC_NAME, 
                                            &functionHandle);
    
	if(returnVal != SUCCESS)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) <<
            "Exported function not found in module : deleteShapeRecognizer " << endl;

        LOG(LTKLogger::LTK_LOGLEVEL_ERR)
				    <<"Error : "<< EDLL_FUNC_ADDRESS <<":"<< getErrorMessage(EDLL_FUNC_ADDRESS)
		            <<" BoxedFieldRecognizer::mapShapeAlgoModuleFunctions" <<endl;

		LTKReturnError(EDLL_FUNC_ADDRESS);
	}

    m_module_deleteShapeRecognizer = (FN_PTR_DELETESHAPERECOGNIZER)functionHandle;
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
		<<"Exiting: BoxedFieldRecognizer::mapShapeAlgoModuleFunctions"
		<<endl;

	return SUCCESS;
}



