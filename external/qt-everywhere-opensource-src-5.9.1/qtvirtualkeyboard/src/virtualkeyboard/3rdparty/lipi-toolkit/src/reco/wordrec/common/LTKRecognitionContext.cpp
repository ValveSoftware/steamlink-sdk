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
 * $LastChangedDate$
 * $Revision$
 * $Author$
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Implementation of LTKRecognitionContext that holds the context
 *             for recognition
 *
 * CONTENTS: 
 *   addTrace 
 *   addTraceGroup 
 *   beginRecoUnit
 *   endRecoUnit 
 *   getAllInk 
 *   getConfidThreshold
 *   getDeviceContext 
 *   getFlag
 *   getLanguageModel 
 *	 getNextBestResults
 *   getNumResults
 *   getScreenContext
 *   getTopResult
 *   setConfidThreshold
 *   setDeviceContext
 *   setFlag
 *   setLanguageModel
 *   setNumResults
 *   setScreenContext 
 *   addRecognitionResult
 *   recognize
 *   reset 
 *
 * AUTHOR:     Deepu V.
 *
 * DATE:       February 22, 2005
 * CHANGE HISTORY:
 * Author		Date			Description of change
 * Thanigai		3-AUG-2005		Added default constructor and setWordRecoEngine 
 *								methods.
 *
 * Deepu       30-AUG-2005      Replaced LTKWordRecoEngine with LTKWordRecognizer
 *                              Changed the representation of m_recognitionFlags 
 *                              since there was a problem with dlls
************************************************************************/

#include "LTKRecognitionContext.h"
#include "LTKMacros.h"
#include "LTKErrors.h"
#include "LTKTrace.h"

#include "LTKErrorsList.h"

#include "LTKTraceGroup.h"

#include "LTKWordRecoResult.h"

#include "LTKWordRecognizer.h"

#include "LTKLoggerUtil.h"

#include "LTKException.h"

/**********************************************************************************
* AUTHOR		: Thanigai
* DATE			: 3-AUG-2005
* NAME			: LTKRecognitionContext
* DESCRIPTION	: Default constructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
LTKRecognitionContext::LTKRecognitionContext()
:m_confidThreshold(0), 
m_numResults(0),
m_nextBestResultIndex(0),
m_wordRecPtr(NULL) 
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKRecognitionContext::LTKRecognitionContext()" << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKRecognitionContext::LTKRecognitionContext()" << endl;
}


/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 22-FEB-2005
* NAME			: LTKRecognitionContext
* DESCRIPTION	: Initialization constructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
LTKRecognitionContext::LTKRecognitionContext(LTKWordRecognizer *wordRecPtr )
:m_wordRecPtr(wordRecPtr),
m_confidThreshold(0), 
m_numResults(0),
m_nextBestResultIndex(0) 

{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKRecognitionContext::LTKRecognitionContext(LTKWordRecognizer*)" << endl;

	if(m_wordRecPtr == NULL)
    {
    	LOG(LTKLogger::LTK_LOGLEVEL_ERR)
            <<"Error : "<< ENULL_POINTER <<":"<< getErrorMessage(ENULL_POINTER)
            <<" LTKRecognitionContext::LTKRecognitionContext(LTKWordRecognizer*)" <<endl;

       throw LTKException(ENULL_POINTER);
    }

	m_recognitionFlags.clear();
	m_wordRecPtr = wordRecPtr;


	
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKRecognitionContext::LTKRecognitionContext(LTKWordRecognizer*)" << endl;
}



/**********************************************************************************
* AUTHOR		: Thanigai
* DATE			: 3-AUG-2005
* NAME			: setWordRecoEngine
* DESCRIPTION	: Accepts the handle to word recognition engine and store it locally
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
int LTKRecognitionContext::setWordRecoEngine(LTKWordRecognizer  *wordRecPtr)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKRecognitionContext::setWordRecoEngine()" << endl;

    if(wordRecPtr == NULL)
    {
    	LOG(LTKLogger::LTK_LOGLEVEL_ERR)
            <<"Error : "<< ENULL_POINTER <<":"<< getErrorMessage(ENULL_POINTER)
            <<" LTKRecognitionContext::setWordRecoEngine()" <<endl;

       LTKReturnError(ENULL_POINTER);
    }
	m_wordRecPtr = wordRecPtr;
	

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKRecognitionContext::setWordRecoEngine()" << endl;

	return SUCCESS;
}



/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 22-FEB-2005
* NAME			: addTrace
* DESCRIPTION	: This function adds a trace to the recognition context for 
*                 recognition
* ARGUMENTS		: trace - the trace to be added
* RETURNS		: SUCCESS/FAILURE
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
int LTKRecognitionContext::addTrace (const LTKTrace& trace)    
{
	int recMode;    //strokes temporary string for getFlag
	string tempStr; // the recognition mode

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKRecognitionContext::addTrace()" << endl;


	m_fieldInk.push_back(trace); //pushing incoming trace to local buffer

	//if the recognition mode is set to streaming mode
	//the recognizer is called at this point

	tempStr = REC_MODE;

	int errorCode;
	
	if((errorCode = getFlag(tempStr,recMode))!=SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
        <<"Error: LTKRecognitionContext::addTrace()"<<endl;

		LTKReturnError(errorCode);
	}
	
	if(recMode ==  REC_MODE_STREAMING)
	{
		m_wordRecPtr->processInk(*this);
	}

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKRecognitionContext::addTrace()" << endl;


	return SUCCESS;

}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 22-FEB-2005
* NAME			: addTraceGroup
* DESCRIPTION	: Adds a vector of tracegroup for recognition in the recognition context
* ARGUMENTS		: fieldInk -  the ink to be added.
* RETURNS		: SUCCESS/FAILURE
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
int LTKRecognitionContext::addTraceGroups (const LTKTraceGroupVector& fieldInk) 
{
	int numTraceGroups = fieldInk.size(); //number of trace groups
	int numTraces =0;  //number of traces in each trace group
	string tempStr;    //strokes temporary string for getFlag
	int recMode =0;      // the recognition mode

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKRecognitionContext::addTraceGroups()" << endl;

	for(int i =0; i<numTraceGroups; ++i)
	{	
		//accessing each trace group
		const LTKTraceGroup& traceGp = fieldInk[i];

		//get all traces from tracegp
		 //const LTKTraceVector& allTraces = traceGp.getAllTraces();
		const LTKTraceVector& allTraces = traceGp.getAllTraces();
		
		//push each trace to local buffer
		numTraces = allTraces.size();
		
		for(int j = 0; j<numTraces; ++j)
		{
			m_fieldInk.push_back(allTraces[j]);
		}

		LOG(LTKLogger::LTK_LOGLEVEL_INFO) << "Pushed Trace Group:"<<i<<endl;

	}

	//if the recognition mode is set to streaming mode
	//the recognizer is called at this point
	tempStr = REC_MODE;

	int errorCode;
	
	if((errorCode = getFlag(tempStr,recMode))!=SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
        <<"Error: LTKRecognitionContext::addTraceGroups()"<<endl;

		LTKReturnError(errorCode);
	}
	
	if(recMode ==  REC_MODE_STREAMING) 
	{
 		m_wordRecPtr->processInk(*this);
	}

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKRecognitionContext::addTraceGroups()" << endl;



	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 28-FEB-2005
* NAME			: beginRecoUnit
* DESCRIPTION	: This function marks the beginning of a recognition unit of Ink.
* ARGUMENTS		: none
* RETURNS		: SUCCESS/FAILURE
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
void LTKRecognitionContext::beginRecoUnit ( ) 
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKRecognitionContext::beginRecoUnit()" << endl;


	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKRecognitionContext::beginRecoUnit()" << endl;

}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 11-MAR-2005
* NAME			: clearRecognitionResult
* DESCRIPTION	: clears all the recognition results
* ARGUMENTS		: none
* RETURNS		: SUCCESS/FAILURE
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
int LTKRecognitionContext::clearRecognitionResult ( )
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKRecognitionContext::clearRecognitionResult()" << endl;
	
	//clearing the results
	m_results.clear();

	//reset the index of next best result
	m_nextBestResultIndex = 0;

	m_fieldInk.clear();

	int errorCode;
	
	if((errorCode=m_wordRecPtr->reset())!=SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
        <<"Error: LTKRecognitionContext::clearRecognitionResult()"<<endl;

		LTKReturnError(errorCode);
	}

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKRecognitionContext::clearRecognitionResult()" << endl;

	return SUCCESS;	
}


/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 28-FEB-2005
* NAME			: endRecoUnit
* DESCRIPTION	: This function marks the ending of a recognition unit of Ink.
* ARGUMENTS		: none
* RETURNS		: SUCCESS/FAILURE
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

void LTKRecognitionContext::endRecoUnit ( )   
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKRecognitionContext::endRecoUnit()" << endl;

	//pushing a "marker" into the stream
	m_fieldInk.push_back(LTKTrace());

	//calling the marker of the recognizer
	m_wordRecPtr->endRecoUnit();

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKRecognitionContext::endRecoUnit()" << endl;
}


/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 28-FEB-2005
* NAME			: getAllInk
* DESCRIPTION	: Access function for the internal Ink data.
* ARGUMENTS		: none
* RETURNS		: reference to internal Ink data
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
const LTKTraceVector& LTKRecognitionContext::getAllInk () const
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKRecognitionContext::getAllInk()" << endl;


	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKRecognitionContext::getAllInk()" << endl;

	return m_fieldInk;

}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 28-FEB-2005
* NAME			: getConfidThreshold
* DESCRIPTION	: Access function for internal confidence threshold
* ARGUMENTS		: none
* RETURNS		: confidence threshold
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
float LTKRecognitionContext::getConfidThreshold ()  const 
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKRecognitionContext::getConfidThreshold()" << endl;


	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKRecognitionContext::getConfidThreshold()" << endl;

	return m_confidThreshold;
}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 28-FEB-2005
* NAME			: getDeviceContext
* DESCRIPTION	: Access function for device context
* ARGUMENTS		: none
* RETURNS		: reference to LTKCapture device 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
const LTKCaptureDevice& LTKRecognitionContext::getDeviceContext ( ) const  
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKRecognitionContext::getDeviceContext()" << endl;


	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKRecognitionContext::getDeviceContext()" << endl;

	return m_deviceContext;
}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 28-FEB-2005
* NAME			: getFlag
* DESCRIPTION	: Returns the value of the flag
* ARGUMENTS		: key - index of map
* RETURNS		: value of queried flag (int)
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

int LTKRecognitionContext::getFlag (const string& key,int& outValue)  const
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKRecognitionContext::getFlag()" << endl;

	if(key=="")
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
    		<<"Error : "<< EEMPTY_STRING <<":"<< getErrorMessage(EEMPTY_STRING)
        	<<" LTKRecognitionContext::getFlag()" <<endl;

		LTKReturnError(EEMPTY_STRING);
	}
	
	vector<pair<string,int> >::const_iterator iter,iterEnd;

	iterEnd = m_recognitionFlags.end();

	//Iterating through the vector to find the key
	for(iter = m_recognitionFlags.begin(); iter != iterEnd; ++iter)
	{
		if( (*iter).first == key )
		{
			outValue = (*iter).second;

		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKRecognitionContext::getFlag()" << endl;
			
			return SUCCESS;
		}
	}

	LOG(LTKLogger::LTK_LOGLEVEL_ERR)
    	<<"Error : "<< EKEY_NOT_FOUND <<":"<< getErrorMessage(EKEY_NOT_FOUND)
        <<" LTKRecognitionContext::getFlag()" <<endl;

	LTKReturnError(EKEY_NOT_FOUND);

}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 28-FEB-2005
* NAME			: getLanguageModel
* DESCRIPTION	: returns the current language model indexed by the key
* ARGUMENTS		: key - index of map
* RETURNS		: value of the queried language model (string)
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

int LTKRecognitionContext::getLanguageModel (const string& key,
													   string& outValue) const
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKRecognitionContext::getLanguageModel()" << endl;

	if(key=="")
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
    		<<"Error : "<< EEMPTY_STRING <<":"<< getErrorMessage(EEMPTY_STRING)
        	<<" LTKRecognitionContext::getLanguageModel()" <<endl;
		
		LTKReturnError(EEMPTY_STRING);
	}
	

	stringStringMap::const_iterator iterMap;

	iterMap = this->m_languageModels.find(key);
	
	if(iterMap != m_languageModels.end() )
	{
		outValue = iterMap->second;
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
	        " Exiting: LTKRecognitionContext::getLanguageModel()" << endl;
		return SUCCESS;
	}


	LOG(LTKLogger::LTK_LOGLEVEL_ERR)
    	<<"Error : "<< EKEY_NOT_FOUND <<":"<< getErrorMessage(EKEY_NOT_FOUND)
        <<" LTKRecognitionContext::getLanguageModel()" <<endl;

	LTKReturnError(EKEY_NOT_FOUND);
	
	

}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 28-FEB-2005
* NAME			: getNextBestResults
* DESCRIPTION	: returns the next N best results
* ARGUMENTS		: numResults - number of results required
*               : results - This will be populated with results
* RETURNS		: SUCCESS/FAILURE
* NOTES			: Maximum number of results added is limited by number of results 
*               : available.  
*				: vector is not cleared inside the function
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
int  LTKRecognitionContext::getNextBestResults (int numResults, 
													LTKWordRecoResultVector& outWordRecResults)
{
	int lastIndex = 0;//Last  index

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKRecognitionContext::getNextBestResults()" << endl;

	if(numResults<=0)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
        	<<"Error : "<< ENON_POSITIVE_NUM <<":"<< getErrorMessage(ENON_POSITIVE_NUM)
            <<" LTKRecognitionContext::getNextBestResults()" <<endl;

		LTKReturnError(ENON_POSITIVE_NUM);
	}

	vector<LTKWordRecoResult>::const_iterator resultBegin, resultEnd, resultIter;

	//Finding index of requested results
	resultBegin = m_results.begin() + m_nextBestResultIndex;

	//Finding index of requested results
	resultEnd = m_results.begin() + m_nextBestResultIndex + numResults;

	if(resultBegin > resultEnd
)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << 
			"Exiting LTKRecognitionContext::getNextBestResults" <<endl;
	
		return SUCCESS;
	}

	//limiting the end to the limits of available results
	if(resultEnd > m_results.end() )
		resultEnd = m_results.end();

	//pushing back the results
	for(resultIter = resultBegin; resultIter< resultEnd; ++resultIter)
	{
		outWordRecResults.push_back(*resultIter);
	}

	//updating next best result index
    m_nextBestResultIndex += numResults;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKRecognitionContext::getNextBestResults()" << endl;

	return SUCCESS;

}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 28-FEB-2005
* NAME			: getNumResults
* DESCRIPTION	: parameter number of results
* ARGUMENTS		: none
* RETURNS		: number of results (int)
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

int LTKRecognitionContext::getNumResults ()  const  
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKRecognitionContext::getNumResults()" << endl;


	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKRecognitionContext::getNumResults()" << endl;

	return m_numResults;
}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 28-FEB-2005
* NAME			: getScreenContext
* DESCRIPTION	: access function for the screen context
* ARGUMENTS		: none
* RETURNS		: reference to screencontext object
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
const LTKScreenContext& LTKRecognitionContext::getScreenContext ( ) const   
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKRecognitionContext::getScreenContext()" << endl;


	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKRecognitionContext::getScreenContext()" << endl;

	return m_screenContext;
}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 28-FEB-2005
* NAME			: getTopResult
* DESCRIPTION	: get the top result from the recognition context
* ARGUMENTS		: result - will be assigned to the top recognition result
* RETURNS		: SUCCESS/FAILURE
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
int LTKRecognitionContext::getTopResult (LTKWordRecoResult& outTopResult)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKRecognitionContext::getTopResult()" << endl;

	if(m_results.size() == 0)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
	          <<"Error : "<< EEMPTY_WORDREC_RESULTS <<":"<< getErrorMessage(EEMPTY_WORDREC_RESULTS)
	          <<" LTKRecognitionContext::getTopResult()" <<endl;

		LTKReturnError(EEMPTY_WORDREC_RESULTS);
	}

	m_nextBestResultIndex = 1;

	//assigning the value to output
	outTopResult = m_results[0];

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKRecognitionContext::getTopResult()" << endl;

 	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 28-FEB-2005
* NAME			: setConfidThreshold
* DESCRIPTION	: set the confidence threshold
* ARGUMENTS		: thresh - the threshold value to be set
* RETURNS		: SUCCESS/FAILURE
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
int LTKRecognitionContext::setConfidThreshold (float thresh)  
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKRecognitionContext::setConfidThreshold()" << endl;

	if(thresh < 0)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
	    	<<"Error : "<< ENEGATIVE_NUM <<":"
	    	<< getErrorMessage(ENEGATIVE_NUM)
	        <<" LTKRecognitionContext::setConfidThreshold()" <<endl;

		LTKReturnError(ENEGATIVE_NUM);
	}

	m_confidThreshold = thresh;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKRecognitionContext::setConfidThreshold()" << endl;

	return SUCCESS;
}


/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 28-FEB-2005
* NAME			: setDeviceContext
* DESCRIPTION	: set the device context
* ARGUMENTS		: dc - reference to device context object to be set
* RETURNS		: SUCCESS/FAILURE
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
void LTKRecognitionContext::setDeviceContext (const LTKCaptureDevice& dc)    
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKRecognitionContext::setDeviceContext()" << endl;

	m_deviceContext = dc;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKRecognitionContext::setDeviceContext()" << endl;



}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 28-FEB-2005
* NAME			: setFlag
* DESCRIPTION	: sets the flag
* ARGUMENTS		: key - index of the flag to be set
*               : value - value of the flag to be set
* RETURNS		: SUCCESS/FAILURE
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

int LTKRecognitionContext::setFlag (const string& key, int value)    
{
	vector<pair<string,int> >::iterator iter,iterEnd;//iterators for iterating through all flags
	
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKRecognitionContext::setFlag()" << endl;

	if(key=="")
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
          <<"Error : "<< EEMPTY_STRING <<":"<< getErrorMessage(EEMPTY_STRING)
          <<" LTKRecognitionContext::setFlag()" <<endl;

		LTKReturnError(EEMPTY_STRING);
	}

	iterEnd = m_recognitionFlags.end();

	//looping through the map to check whether the flag exists
	for(iter = m_recognitionFlags.begin(); iter != iterEnd; ++iter)
	{
		if((*iter).first == key)
		{
			(*iter).second = value;
			break;
		}
	}
	
	//if the flag is not there in the map add a new flag
	if((iter == iterEnd)||(m_recognitionFlags.empty()) )
	{
		m_recognitionFlags.push_back(pair<string,int>(key,value));
	}

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKRecognitionContext::setFlag()" << endl;

	return SUCCESS;

}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 28-FEB-2005
* NAME			: setLanguageModel
* DESCRIPTION	: sets the language model 
* ARGUMENTS		: property - name of ppty to be set (DICTIONARY, GRAMMAR)
*               : value - value to be set
* RETURNS		: SUCCESS/FAILURE
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
int LTKRecognitionContext::setLanguageModel (const string& property, const string& value)  
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKRecognitionContext::setLanguageModel()" << endl;

	if(property=="" || value=="")
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
			"Either property or value is empty"<<endl;
		
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
          <<"Error : "<< EEMPTY_STRING <<":"<< getErrorMessage(EEMPTY_STRING)
          <<" LTKRecognitionContext::setLanguageModel()" <<endl;

		LTKReturnError(EEMPTY_STRING);
	}

	m_languageModels [property] = value;
	
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKRecognitionContext::setLanguageModel()" << endl;

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 28-FEB-2005
* NAME			: setNumResults
* DESCRIPTION	: sets parameter number of results to be buffered from recognizer
* ARGUMENTS		: numResults - the value to be set
* RETURNS		: SUCCESS/FAILURE
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

int LTKRecognitionContext::setNumResults (int numResults)  
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKRecognitionContext::setNumResults()" << endl;

	if(numResults <= 0)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
          <<"Error : "<< ENON_POSITIVE_NUM <<":"
          << getErrorMessage(ENON_POSITIVE_NUM)
          <<" LTKRecognitionContext::setNumResults()" <<endl;
		
		LTKReturnError(ENON_POSITIVE_NUM);
	}

	m_numResults = numResults;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKRecognitionContext::setNumResults()" << endl;

	return SUCCESS;
}


/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 28-FEB-2005
* NAME			: setScreenContext
* DESCRIPTION	: sc - reference to the screencontext object to be set
* ARGUMENTS		: numResults - the value to be set
* RETURNS		: SUCCESS/FAILURE
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
void LTKRecognitionContext::setScreenContext (const LTKScreenContext& sc)    
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKRecognitionContext::setScreenContext()" << endl;

	m_screenContext = sc;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKRecognitionContext::setScreenContext()" << endl;
}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 11-MAR-2005
* NAME			: addRecognitionResult
* DESCRIPTION	: used by the recognizer to set the results back in the recognition context
* ARGUMENTS		: result - the value to be added
* RETURNS		: SUCCESS/FAILURE
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
void LTKRecognitionContext::addRecognitionResult (const LTKWordRecoResult& result)
{
	
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKRecognitionContext::addRecognitionResult()" << endl;

	//adding the result to the internal data structure
    m_results.push_back(result);

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKRecognitionContext::addRecognitionResult()" << endl;
}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 11-MAR-2005
* NAME			: recognize
* DESCRIPTION	: the recognize call from the application.
*               : calls the recognize emthod of the recognizer
* ARGUMENTS		: numResults - the value to be set
* RETURNS		: SUCCESS/FAILURE
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
int LTKRecognitionContext::recognize ()
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKRecognitionContext::recognize()" << endl;

	int errorCode;

	//calling recognize method of the recognizer
	if(m_wordRecPtr!=NULL)
	{
		if( (errorCode = m_wordRecPtr->recognize(*this)) != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
	        	<<"Error: LTKRecognitionContext::recognize()"<<endl;
		
			LTKReturnError(errorCode);
		}
			
	}
	else
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Recognizer is not initialized" <<endl;

		 LOG(LTKLogger::LTK_LOGLEVEL_ERR)
		      <<"Error : "<< ENULL_POINTER <<":"<< getErrorMessage(ENULL_POINTER)
		      <<" LTKRecognitionContext::recognize()" <<endl;


		LTKReturnError(ENULL_POINTER);
	}

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKRecognitionContext::recognize()" << endl;

	return SUCCESS;
}


/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 28-FEB-2005
* NAME			: reset
* DESCRIPTION	: Reset various parameters.
* ARGUMENTS		: resetParam - specifies data to be rest
* RETURNS		: SUCCESS/FAILURE
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
/**
* This function is used to reset the different components of recognition context
* @param resetParam : parameter that identifies the component to be reset
* @return SUCCESS/FAILURE
*/
int LTKRecognitionContext::reset (int resetParam) 
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKRecognitionContext::reset()" << endl;

	if(resetParam & LTK_RST_INK)
    {
	   m_fieldInk.clear();
    }

	if(resetParam & LTK_RST_RECOGNIZER)
	{
		int errorCode=0;
		
		if((errorCode=m_wordRecPtr->reset(resetParam))!=SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
		        <<"Error: LTKRecognitionContext::reset()"<<endl;

			LTKReturnError(errorCode);
		}
	}

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKRecognitionContext::reset()" << endl;
 
	return SUCCESS;
}


/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 22-FEB-2005
* NAME			: LTKRecognitionContext
* DESCRIPTION	: Destructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
LTKRecognitionContext::~LTKRecognitionContext()
{

}
